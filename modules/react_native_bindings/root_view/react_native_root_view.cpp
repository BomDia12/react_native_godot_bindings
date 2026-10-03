#include "react_native_root_view.h"

#include "../fabric/fabric_ui_manager.h"
#include "../mounting/rn_mounting_manager.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/viewport.h"

ReactNativeRootView::ReactNativeRootView() {
	mounting_manager = std::make_unique<RNMountingManager>(this);
	set_mouse_filter(Control::MOUSE_FILTER_PASS);
	set_process_input(true);
	set_notify_transform(true);
}

ReactNativeRootView::~ReactNativeRootView() {
	if (registered) {
		if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
			coordinator->unregister_root(this);
		}
	}
}

void ReactNativeRootView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("mount", "tree"), &ReactNativeRootView::mount);
	ClassDB::bind_method(D_METHOD("get_root_tag"), &ReactNativeRootView::get_root_tag);
	ClassDB::bind_method(D_METHOD("set_application_key", "application_key"), &ReactNativeRootView::set_application_key);
	ClassDB::bind_method(D_METHOD("get_application_key"), &ReactNativeRootView::get_application_key);
	ClassDB::bind_method(D_METHOD("reload"), &ReactNativeRootView::reload);
#ifdef DEBUG_ENABLED
	ClassDB::bind_method(D_METHOD("set_mount_failure_injection", "before_mutation", "after_mutation"), &ReactNativeRootView::set_mount_failure_injection);
#endif
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "application_key"), "set_application_key", "get_application_key");
}

void ReactNativeRootView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_viewport()->connect("gui_input_dispatched", callable_mp(this, &ReactNativeRootView::_on_gui_input_dispatched));
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ReactNativeRootView::_invalidate_host_layout));
			if (!registered) {
				if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
					registered = true;
					coordinator->register_root(this);
				}
			}
		} break;
		case NOTIFICATION_RESIZED: {
			if (mounting_manager->get_committed_root().is_valid()) {
				const std::shared_ptr<const RNSurfaceSnapshot> old_snapshot = mounting_manager->get_snapshot();
				Vector<RNNativeEvent> events;
				String error;
				transaction_in_flight = true;
				const bool resized = mounting_manager->resize(get_size(), get_global_transform_with_canvas(), events, error);
				transaction_in_flight = false;
				if (resized) {
					_publish_mounted_result(events, old_snapshot);
				} else if (!error.is_empty()) {
					ERR_PRINT(vformat("React Native surface %d resize failed: %s", root_tag, error));
				}
			}
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			_publish_transform_snapshot();
		} break;
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			_invalidate_host_layout();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			get_viewport()->disconnect("gui_input_dispatched", callable_mp(this, &ReactNativeRootView::_on_gui_input_dispatched));
			ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &ReactNativeRootView::_invalidate_host_layout));
			if (registered) {
				if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
					coordinator->unregister_root(this);
				}
				registered = false;
			}
		} break;
	}
}

void ReactNativeRootView::set_application_key(const String &p_key) {
	ERR_FAIL_COND_MSG(p_key.is_empty(), "ReactNativeRootView.application_key cannot be empty.");
	if (application_key == p_key) {
		return;
	}
	application_key = p_key;
	if (registered) {
		if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
			coordinator->application_key_changed(this);
		}
	}
}

void ReactNativeRootView::reload() {
	if (!registered) {
		return;
	}
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		coordinator->reload_root(this);
	}
}

void ReactNativeRootView::set_mount_failure_injection(int p_before_mutation, int p_after_mutation) {
#ifdef DEBUG_ENABLED
	mounting_manager->set_failure_injection(p_before_mutation, p_after_mutation);
#else
	(void)p_before_mutation;
	(void)p_after_mutation;
#endif
}

void ReactNativeRootView::_attach_surface(const RNSurfaceRoute &p_route) {
	root_tag = p_route.root_tag;
	runtime_generation = p_route.runtime_generation;
	surface_epoch = p_route.surface_epoch;
	mounted_revision = 0;
	mounting_manager->attach(runtime_generation, root_tag, surface_epoch);
}

Vector<RNNativeEvent> ReactNativeRootView::_prepare_surface_stop() {
	std::shared_ptr<const RNSurfaceSnapshot> snapshot = mounting_manager->get_snapshot();
	Vector<RNNativeEvent> events = input_router.cancel_all(snapshot.get(), root_tag, runtime_generation);
	if (focused_tag != 0) {
		RNNativeEvent blur;
		blur.tag = focused_tag;
		blur.name = "topBlur";
		blur.priority = FabricUIManager::EVENT_PRIORITY_DISCRETE;
		blur.generation = runtime_generation;
		events.push_back(blur);
	}
	_stamp_events(events);
	return events;
}

void ReactNativeRootView::_detach_surface(int p_root_tag, uint64_t p_epoch) {
	if (root_tag != p_root_tag || surface_epoch != p_epoch) {
		return;
	}
	_clear_scene_state(false);
	root_tag = 0;
	runtime_generation = 0;
	surface_epoch = 0;
	mounted_revision = 0;
}

void ReactNativeRootView::_clear_scene_state(bool p_keep_container) {
	input_router.clear();
	transaction_in_flight = true;
	mounting_manager->clear(p_keep_container);
	transaction_in_flight = false;
	focused_tag = 0;
}

void ReactNativeRootView::_stamp_events(Vector<RNNativeEvent> &r_events) const {
	for (RNNativeEvent &event : r_events) {
		event.root_tag = root_tag;
		event.generation = runtime_generation;
		event.surface_epoch = surface_epoch;
	}
}

void ReactNativeRootView::_enqueue_events(Vector<RNNativeEvent> p_events) {
	_stamp_events(p_events);
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		coordinator->enqueue_events(p_events);
	}
}

void ReactNativeRootView::mount(const Ref<RNShadowNode> &p_tree) {
	if (p_tree.is_null()) {
		return;
	}
	RNPendingCommit commit;
	commit.runtime_generation = runtime_generation;
	commit.root_tag = root_tag;
	commit.surface_epoch = surface_epoch;
	commit.revision = mounted_revision + 1;
	commit.tree = p_tree;
	_accept_commit(commit);
}

void ReactNativeRootView::_accept_commit(const RNPendingCommit &p_commit) {
	if (!is_inside_tree() || p_commit.runtime_generation != runtime_generation || p_commit.root_tag != root_tag || p_commit.surface_epoch != surface_epoch || p_commit.revision <= mounted_revision) {
		return;
	}
	Vector<RNNativeEvent> events;
	String error;
	const std::shared_ptr<const RNSurfaceSnapshot> old_snapshot = mounting_manager->get_snapshot();
	transaction_in_flight = true;
	const bool applied = mounting_manager->commit(p_commit, get_size(), get_global_transform_with_canvas(), events, error);
	transaction_in_flight = false;
	if (!applied) {
		descriptor_events.clear();
		if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
			coordinator->reject_commit(root_tag, surface_epoch, p_commit.revision, error);
		}
		return;
	}
	for (const RNNativeEvent &event : descriptor_events) {
		events.push_back(event);
	}
	descriptor_events.clear();
	mounted_revision = p_commit.revision;
	_publish_mounted_result(events, old_snapshot);
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		coordinator->mark_surface_mounted(root_tag, surface_epoch, mounted_revision);
	}
}

void ReactNativeRootView::_publish_mounted_result(Vector<RNNativeEvent> p_events, const std::shared_ptr<const RNSurfaceSnapshot> &p_old_snapshot) {
	ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton();
	std::shared_ptr<const RNSurfaceSnapshot> snapshot = mounting_manager->get_snapshot();
	if (coordinator && snapshot) {
		coordinator->publish_snapshot(snapshot);
	}
	Vector<RNNativeEvent> input_events = snapshot ? input_router.reconcile_snapshot(p_old_snapshot.get(), *snapshot, root_tag, runtime_generation) : Vector<RNNativeEvent>();
	for (const RNNativeEvent &event : input_events) {
		p_events.push_back(event);
	}
	_enqueue_events(p_events);
	if (focused_tag != 0) {
		Control *focused = mounting_manager->focus_control(focused_tag);
		if (!focused || (focused->get_focus_mode() != Control::FOCUS_CLICK && focused->get_focus_mode() != Control::FOCUS_ALL)) {
			_set_focused_tag(0, p_old_snapshot.get());
		} else if (!focused->get_viewport()->gui_get_focus_owner()) {
			focused->grab_focus();
		}
	}
	mounting_manager->activate_published_hosts();
}

void ReactNativeRootView::_publish_transform_snapshot() {
	if (root_tag == 0 || mounting_manager->get_committed_root().is_null()) {
		return;
	}
	const auto previous = mounting_manager->get_snapshot();
	mounting_manager->publish_transform(get_global_transform_with_canvas());
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		coordinator->publish_snapshot(mounting_manager->get_snapshot());
	}
	_enqueue_events(input_router.reconcile_snapshot(previous.get(), *mounting_manager->get_snapshot(), root_tag, runtime_generation));
}

void ReactNativeRootView::_invalidate_host_layout() {
	++native_resource_revision;
	if (!native_layout_pending) {
		native_layout_pending = true;
		callable_mp(this, &ReactNativeRootView::_refresh_native_dependencies).call_deferred();
	}
}

void ReactNativeRootView::_refresh_native_dependencies() {
	native_layout_pending = false;
	if (!is_inside_tree() || mounting_manager->get_committed_root().is_null()) {
		return;
	}
	if (transaction_in_flight || mounting_manager->is_transaction_in_flight()) {
		_invalidate_host_layout();
		return;
	}
	const auto previous = mounting_manager->get_snapshot();
	Vector<RNNativeEvent> events;
	String error;
	transaction_in_flight = true;
	const bool refreshed = mounting_manager->resize(get_size(), get_global_transform_with_canvas(), events, error);
	transaction_in_flight = false;
	if (refreshed) {
		_publish_mounted_result(events, previous);
	} else {
		ERR_PRINT(error);
	}
}

void ReactNativeRootView::_emit_host_event(uint64_t p_generation, uint64_t p_epoch, int p_tag, ObjectID p_host, const StringName &p_name, const Dictionary &p_payload, uint64_t p_revision) {
	if (p_generation != runtime_generation || p_epoch != surface_epoch || !mounting_manager->is_current_host(p_tag, p_host, p_revision)) {
		return;
	}
	Ref<RNShadowNode> node = mounting_manager->get_registry().get_shadow_node(p_tag);
	if (node.is_null() || !node->descriptor) {
		return;
	}
	const Dictionary config = node->descriptor->get_view_config();
	const Dictionary direct = config.get("directEventTypes", Dictionary());
	const Dictionary bubbling = config.get("bubblingEventTypes", Dictionary());
	if (!direct.has(p_name) && !bubbling.has(p_name)) {
		ERR_PRINT(vformat("E_UNDECLARED_EVENT: %s.%s", node->view_name, p_name));
		return;
	}
	RNNativeEvent event;
	event.tag = p_tag;
	event.name = p_name;
	event.payload = p_payload.duplicate(true);
	event.payload["target"] = p_tag;
	event.retained_target = node->event_target;
	if (p_name == "topFocus") {
		focused_tag = p_tag;
	} else if (p_name == "topBlur" && focused_tag == p_tag) {
		focused_tag = 0;
	}
	event.priority = FabricUIManager::EVENT_PRIORITY_DEFAULT;
	_enqueue_events({ event });
}

void ReactNativeRootView::_invalidate_host_geometry() {
	if (!transaction_in_flight && !mounting_manager->is_transaction_in_flight()) {
		_publish_transform_snapshot();
	}
}

bool ReactNativeRootView::get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const {
	std::shared_ptr<const RNSurfaceSnapshot> snapshot = mounting_manager->get_snapshot();
	const RNMountedNodeSnapshot *node = snapshot ? snapshot->nodes.getptr(p_tag) : nullptr;
	if (!node) {
		return false;
	}
	r_local_rect = node->local_rect;
	r_page_position = node->root_rect.position;
	return true;
}

bool ReactNativeRootView::_apply_imperative(const RNImperativeRequest &p_request) {
	if (p_request.runtime_generation != runtime_generation || p_request.root_tag != root_tag || p_request.surface_epoch != surface_epoch) {
		return false;
	}
	Ref<RNShadowNode> node = mounting_manager->get_registry().get_shadow_node(p_request.tag);
	if (node.is_null() || node->view_name != p_request.component_name) {
		return false;
	}
	if (p_request.kind == RNImperativeRequestKind::COMMAND) {
		if (p_request.command_name == "hotspotUpdate" || p_request.command_name == "setPressed") {
			WARN_PRINT_ONCE(vformat("View command %s is not supported by the Godot host.", p_request.command_name));
			return false;
		}
		String command_error;
		if (!mounting_manager->dispatch_command(p_request.tag, p_request.command_name, p_request.payload, command_error)) {
			WARN_PRINT(command_error);
			return false;
		}
		return true;
	}
	if (p_request.payload.get_type() != Variant::DICTIONARY) {
		return false;
	}
	Vector<RNNativeEvent> events;
	String error;
	const std::shared_ptr<const RNSurfaceSnapshot> old_snapshot = mounting_manager->get_snapshot();
	transaction_in_flight = true;
	const bool applied = mounting_manager->apply_direct_props(p_request.tag, Dictionary(p_request.payload), get_size(), get_global_transform_with_canvas(), events, error);
	transaction_in_flight = false;
	if (!applied) {
		descriptor_events.clear();
		ERR_PRINT(vformat("React Native direct props failed for surface %d tag %d: %s", root_tag, p_request.tag, error));
		return false;
	}
	for (const RNNativeEvent &event : descriptor_events) {
		events.push_back(event);
	}
	descriptor_events.clear();
	_publish_mounted_result(events, old_snapshot);
	return false;
}

void ReactNativeRootView::_on_descriptor_value_changed(double p_value, int p_tag, ObjectID p_control_id) {
	if (mounting_manager->get_registry().get_tag(p_control_id) != p_tag) {
		return;
	}
	RNNativeEvent event;
	event.tag = p_tag;
	event.name = "topValueChanged";
	event.priority = FabricUIManager::EVENT_PRIORITY_DEFAULT;
	event.payload["value"] = p_value;
	if (transaction_in_flight) {
		descriptor_events.push_back(event);
	} else {
		Vector<RNNativeEvent> events;
		events.push_back(event);
		_enqueue_events(events);
	}
}

void ReactNativeRootView::_flush_imperative_updates() {
}

void ReactNativeRootView::_on_focus_entered(int p_tag, ObjectID p_control_id) {
	if (transaction_in_flight) {
		return;
	}
	if (mounting_manager->get_registry().get_tag(p_control_id) == p_tag) {
		_set_focused_tag(p_tag);
	}
}

void ReactNativeRootView::_on_focus_exited(int p_tag, ObjectID p_control_id) {
	if (transaction_in_flight) {
		return;
	}
	if (mounting_manager->get_registry().get_tag(p_control_id) == p_tag && focused_tag == p_tag) {
		_set_focused_tag(0);
	}
}

void ReactNativeRootView::_set_focused_tag(int p_tag, const RNSurfaceSnapshot *p_old_snapshot) {
	if (focused_tag == p_tag) {
		return;
	}
	Vector<RNNativeEvent> events;
	if (focused_tag != 0) {
		RNNativeEvent blur;
		blur.tag = focused_tag;
		blur.name = "topBlur";
		blur.priority = FabricUIManager::EVENT_PRIORITY_DISCRETE;
		if (p_old_snapshot) {
			const RNMountedNodeSnapshot *old_node = p_old_snapshot->nodes.getptr(focused_tag);
			if (old_node && old_node->shadow_node.is_valid()) {
				blur.retained_target = old_node->shadow_node->event_target;
			}
		}
		events.push_back(blur);
	}
	focused_tag = p_tag;
	if (focused_tag != 0) {
		RNNativeEvent focus;
		focus.tag = focused_tag;
		focus.name = "topFocus";
		focus.priority = FabricUIManager::EVENT_PRIORITY_DISCRETE;
		events.push_back(focus);
	}
	_enqueue_events(events);
}

void ReactNativeRootView::input(const Ref<InputEvent> &) {}

void ReactNativeRootView::_on_gui_input_dispatched(const Ref<InputEvent> &p_event, uint64_t p_control_id, uint64_t p_event_id) {
	(void)p_event_id;
	Control *selected = Object::cast_to<Control>(ObjectDB::get_instance(ObjectID(p_control_id)));
	const int tag = mounting_manager->tag_for_input_control(selected);
	if (tag != 0 || Ref<InputEventMouseMotion>(p_event).is_valid()) {
		_route_input(p_event, tag, selected ? selected->get_viewport() : get_viewport());
	}
}

void ReactNativeRootView::_route_input(const Ref<InputEvent> &p_event, int p_native_tag, Viewport *p_viewport) {
	std::shared_ptr<const RNSurfaceSnapshot> snapshot = mounting_manager->get_snapshot();
	if (root_tag == 0 || !snapshot) {
		return;
	}
	RNInputRouter::RouteResult result;
	if (Ref<InputEventKey> key = p_event; key.is_valid()) {
		Control *focus_owner = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
		if (!p_native_tag) {
			p_native_tag = focus_owner ? mounting_manager->tag_for_input_control(focus_owner) : 0;
		}
		result = input_router.route_key(key, p_native_tag, runtime_generation);
	} else {
		Point2 screen_position;
		if (Ref<InputEventMouse> mouse = p_event; mouse.is_valid()) {
			screen_position = mouse->get_position();
		} else if (Ref<InputEventScreenTouch> touch = p_event; touch.is_valid()) {
			screen_position = touch->get_position();
		} else if (Ref<InputEventScreenDrag> drag = p_event; drag.is_valid()) {
			screen_position = drag->get_position();
		} else {
			return;
		}
		screen_position = (p_viewport ? p_viewport : get_viewport())->get_screen_transform().xform(screen_position);
		const Transform2D transform = get_screen_transform();
		if (Math::is_zero_approx(transform.determinant())) {
			return;
		}
		const Point2 root_position = transform.affine_inverse().xform(screen_position);
		result = input_router.route_pointer(p_event, *snapshot, root_tag, runtime_generation, root_position, screen_position, p_native_tag);
	}
	if (mounting_manager->owns_native_activation(p_native_tag)) {
		Vector<RNNativeEvent> observed;
		for (const RNNativeEvent &event : result.events) {
			if (!String(event.name).begins_with("topTouch") && event.name != "topClick") {
				observed.push_back(event);
			}
		}
		result.events = observed;
		result.focus_tag = 0;
	}
	_enqueue_events(result.events);
	if (result.focus_tag != 0) {
		Control *control = mounting_manager->focus_control(result.focus_tag);
		if (control && (control->get_focus_mode() == Control::FOCUS_CLICK || control->get_focus_mode() == Control::FOCUS_ALL)) {
			control->grab_focus();
		} else if (Control *focused = mounting_manager->focus_control(focused_tag)) {
			focused->release_focus();
		}
	}
}

void ReactNativeRootView::_cancel_host_input() {
	const auto snapshot = mounting_manager->get_snapshot();
	_enqueue_events(input_router.cancel_all(snapshot.get(), root_tag, runtime_generation));
	if (auto *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		_enqueue_events(coordinator->get_state()->pointer_capture.clear_surface(root_tag, surface_epoch, runtime_generation));
	}
}
