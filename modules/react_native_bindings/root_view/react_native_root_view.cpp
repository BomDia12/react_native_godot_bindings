#include "react_native_root_view.h"

#include "../fabric/fabric_ui_manager.h"
#include "../fabric/rn_layout.h"
#include "../fabric/rn_view_style.h"
#include "../singletons/hermes_runtime_singleton.h"
#include "../singletons/react_native_file_singleton.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/string/print_string.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/main/viewport.h"

namespace {
constexpr const char *RUN_APPLICATION_FUNCTION = "__godotRunApplication";
constexpr const char *FLUSH_TIMERS_FUNCTION = "__godotFlushTimers";
} //namespace

ReactNativeRootView::ReactNativeRootView() {
	set_mouse_filter(Control::MOUSE_FILTER_PASS);
	set_process_input(true);
	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && !file_singleton->is_connected("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed))) {
		file_singleton->connect("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed));
	}
}

ReactNativeRootView::~ReactNativeRootView() {
	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && file_singleton->is_connected("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed))) {
		file_singleton->disconnect("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed));
	}

	registry.clear();
}

void ReactNativeRootView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("mount", "tree"), &ReactNativeRootView::mount);
	ClassDB::bind_method(D_METHOD("_flush_mounts"), &ReactNativeRootView::_flush_mounts);
	ClassDB::bind_method(D_METHOD("_flush_native_events"), &ReactNativeRootView::_flush_native_events);
	ClassDB::bind_method(D_METHOD("get_root_tag"), &ReactNativeRootView::get_root_tag);
}

void ReactNativeRootView::_notification(int p_what) {
	switch (p_what) {
		// ENTER_TREE rather than READY: READY fires once per node lifetime, so a root
		// view that is reparented or re-added would never boot its runtime again.
		case NOTIFICATION_ENTER_TREE: {
			_boot();
		} break;

		case NOTIFICATION_PROCESS: {
			if (has_timers) {
				HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
				if (hermes) {
					hermes->call_function(FLUSH_TIMERS_FUNCTION);
				}
			}
		} break;

		case NOTIFICATION_RESIZED: {
			if (committed_tree.is_valid()) {
				_layout_and_mount();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_cleanup_runtime_state(true);
			if (booted) {
				booted = false;
				if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
					hermes->reset();
				}
			}
		} break;
	}
}

void ReactNativeRootView::_boot() {
	if (booted) {
		return;
	}

	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL_MSG(hermes, "ReactNativeRootView: HermesRuntime singleton is missing.");

	root_tag = registry.allocate_tag();
	ui_manager = std::make_shared<FabricUIManager>(get_instance_id(), hermes->get_runtime_generation());
	hermes->install_host_object(FabricUIManager::GLOBAL_NAME, ui_manager);
	booted = true;

	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && file_singleton->has_file()) {
		_reload_from_source(file_singleton->get_file_content());
	}
}

void ReactNativeRootView::_reload_from_source(const String &p_source) {
	if (p_source.is_empty()) {
		return;
	}

	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL(hermes);

	_cleanup_runtime_state(true);
	hermes->reset();

	hermes->set_global("__godotRootTag", root_tag);
	hermes->evaluate(p_source, "godot://bundle.js");

	const String error = hermes->get_last_error();
	if (!error.is_empty()) {
		ERR_PRINT("ReactNativeRootView: " + error);
		return;
	}

	has_timers = hermes->get_global(FLUSH_TIMERS_FUNCTION).get_type() != Variant::NIL;
	set_process(has_timers);

	if (hermes->get_global(RUN_APPLICATION_FUNCTION).get_type() != Variant::NIL) {
		Array args;
		args.push_back(root_tag);
		hermes->call_function(RUN_APPLICATION_FUNCTION, args);

		const String run_error = hermes->get_last_error();
		if (!run_error.is_empty()) {
			ERR_PRINT("ReactNativeRootView: " + run_error);
		}
	}
}

void ReactNativeRootView::_cleanup_runtime_state(bool p_dispatch_cancellations) {
	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	if (ui_manager && p_dispatch_cancellations && hermes) {
		_enqueue_events(input_router.cancel_all(committed_tree, root_tag, ui_manager->get_runtime_generation()), false);
		hermes->dispatch_queued_events(ui_manager);
	}
	input_router.clear();
	if (ui_manager) {
		ui_manager->clear_events();
	}
	pending_commits.clear();
	mount_flush_scheduled = false;
	committed_tree.unref();
	registry.clear();
	layout_cache.clear();
	focused_tag = 0;
	_clear_children();
}

void ReactNativeRootView::_clear_children() {
	for (int i = get_child_count() - 1; i >= 0; --i) {
		Node *child = get_child(i);
		remove_child(child);
		child->queue_free();
	}
}

void ReactNativeRootView::_on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists) {
	(void)p_path;

	if (!booted || !p_exists || p_content.is_empty()) {
		return;
	}

	_reload_from_source(p_content);
}

void ReactNativeRootView::mount(const Ref<RNShadowNode> &p_tree) {
	// The single gate for every recursive walk below: layout, mounting, hit testing and
	// measurement all trust that the committed tree fits on the native stack.
	if (!RNShadowNode::is_within_depth_limit(p_tree)) {
		ERR_PRINT(vformat("ReactNativeRootView: shadow tree is nested deeper than %d; refusing to mount it.", RNShadowNode::MAX_DEPTH));
		return;
	}

	committed_tree = p_tree;
	_layout_and_mount();
}

void ReactNativeRootView::enqueue_mount(const Ref<RNShadowNode> &p_tree, uint64_t p_generation) {
	PendingCommit commit;
	commit.tree = p_tree;
	commit.generation = p_generation;
	pending_commits.push_back(commit);
	if (!mount_flush_scheduled) {
		mount_flush_scheduled = true;
		call_deferred("_flush_mounts");
	}
}

void ReactNativeRootView::_flush_mounts() {
	mount_flush_scheduled = false;
	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	if (!hermes || !is_inside_tree()) {
		pending_commits.clear();
		return;
	}
	// Only the newest tree is observable, and mounting is a full Yoga pass plus a Control
	// rebuild, so the commits it superseded are dropped rather than rendered.
	const uint64_t generation = hermes->get_runtime_generation();
	Ref<RNShadowNode> newest_tree;
	for (const PendingCommit &commit : pending_commits) {
		if (commit.generation == generation) {
			newest_tree = commit.tree;
		}
	}
	pending_commits.clear();
	if (newest_tree.is_valid()) {
		mount(newest_tree);
	}
}

void ReactNativeRootView::_flush_native_events() {
	if (!ui_manager) {
		return;
	}
	if (HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton()) {
		hermes->dispatch_queued_events(ui_manager);
	}
}

void ReactNativeRootView::_enqueue_events(const Vector<RNNativeEvent> &p_events, bool p_request_flush) {
	if (!ui_manager) {
		return;
	}
	for (const RNNativeEvent &event : p_events) {
		ui_manager->enqueue_event(event);
	}
	if (p_request_flush && !p_events.is_empty()) {
		ui_manager->request_event_flush();
	}
}

void ReactNativeRootView::_layout_and_mount() {
	if (committed_tree.is_null()) {
		return;
	}

	RNLayout::calculate(committed_tree, get_size());

	registry.clear();
	_clear_children();
	_build_node(committed_tree, this);

	const uint64_t generation = ui_manager ? ui_manager->get_runtime_generation() : 0;
	_enqueue_events(input_router.reconcile_tree(committed_tree, registry, root_tag, generation));
	HashMap<int, Rect2> next_layout_cache;
	_queue_layout_events(committed_tree, next_layout_cache, generation);
	layout_cache = next_layout_cache;

	if (focused_tag != 0) {
		Control *focused = Object::cast_to<Control>(registry.get_node(focused_tag));
		if (focused && focused->get_focus_mode() != Control::FOCUS_NONE) {
			focused->grab_focus();
		} else {
			_set_focused_tag(0);
		}
	}
}

Control *ReactNativeRootView::_build_node(const Ref<RNShadowNode> &p_node, Control *p_parent, bool p_branch_targetable) {
	if (p_node.is_null() || p_node->view_name == "RCTRawText") {
		return nullptr;
	}

	if (p_node->view_name == "RCTRootView") {
		for (const Ref<RNShadowNode> &child : p_node->children) {
			_build_node(child, p_parent, p_branch_targetable);
		}
		return nullptr;
	}

	if (p_node->view_name == "RCTText") {
		Label *label = memnew(Label);
		label->set_position(p_node->layout.position);
		label->set_size(p_node->layout.size);
		label->set_text(p_node->collect_text());

		float font_size = 0.0f;
		if (RNViewStyle::font_size_of(p_node->props, font_size)) {
			label->add_theme_font_size_override("font_size", int(font_size));
		}
		Color color;
		if (RNViewStyle::color_of(p_node->props, "color", color)) {
			label->add_theme_color_override("font_color", color);
		}
		label->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_node->props)));

		p_parent->add_child(label);
		registry.register_node(p_node->tag, label, p_node);
		return label;
	}

	if (p_node->view_name != "RCTView") {
		WARN_PRINT(vformat("Mounting unrecognized view \"%s\" as a plain View.", p_node->view_name));
	}

	Panel *panel = memnew(Panel);
	panel->set_position(p_node->layout.position);
	panel->set_size(p_node->layout.size);
	panel->add_theme_style_override("panel", RNViewStyle::build_stylebox(p_node->props));
	panel->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_node->props)));
	panel->set_clip_contents(RNViewStyle::clips_contents(p_node->props));
	const String pointer_events = String(p_node->props.get("pointerEvents", "auto")).to_lower();
	const bool branch_enabled = p_branch_targetable && pointer_events != "none";
	const bool self_targetable = branch_enabled && pointer_events != "box-none";
	const bool descendants_targetable = branch_enabled && pointer_events != "box-only";
	panel->set_mouse_filter(branch_enabled ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
	panel->set_focus_mode(self_targetable && bool(p_node->props.get("focusable", false)) ? Control::FOCUS_ALL : Control::FOCUS_NONE);

	p_parent->add_child(panel);
	registry.register_node(p_node->tag, panel, p_node);
	panel->connect("focus_entered", callable_mp(this, &ReactNativeRootView::_on_focus_entered).bind(p_node->tag, panel->get_instance_id()));
	panel->connect("focus_exited", callable_mp(this, &ReactNativeRootView::_on_focus_exited).bind(p_node->tag, panel->get_instance_id()));

	for (const Ref<RNShadowNode> &child : p_node->children) {
		_build_node(child, panel, descendants_targetable);
	}
	return panel;
}

void ReactNativeRootView::_queue_layout_events(const Ref<RNShadowNode> &p_node, HashMap<int, Rect2> &r_next_cache, uint64_t p_generation) {
	if (p_node.is_null()) {
		return;
	}
	if (p_node->view_name == "RCTView" || p_node->view_name == "RCTText") {
		r_next_cache[p_node->tag] = p_node->layout;
		const Rect2 *previous = layout_cache.getptr(p_node->tag);
		if (p_node->props.has("onLayout") && (!previous || !previous->is_equal_approx(p_node->layout))) {
			Dictionary rectangle;
			rectangle["x"] = p_node->layout.position.x;
			rectangle["y"] = p_node->layout.position.y;
			rectangle["width"] = p_node->layout.size.x;
			rectangle["height"] = p_node->layout.size.y;
			Dictionary payload;
			payload["layout"] = rectangle;
			RNNativeEvent event;
			event.tag = p_node->tag;
			event.name = "topLayout";
			event.priority = FabricUIManager::EVENT_PRIORITY_DEFAULT;
			event.generation = p_generation;
			event.payload = payload;
			Vector<RNNativeEvent> events;
			events.push_back(event);
			_enqueue_events(events);
		}
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		_queue_layout_events(child, r_next_cache, p_generation);
	}
}

bool ReactNativeRootView::_find_page_position(const Ref<RNShadowNode> &p_node, int p_tag, const Point2 &p_parent_position, Point2 &r_position) {
	if (p_node.is_null()) {
		return false;
	}
	const Point2 position = p_parent_position + p_node->layout.position;
	if (p_node->tag == p_tag) {
		r_position = position;
		return true;
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (_find_page_position(child, p_tag, position, r_position)) {
			return true;
		}
	}
	return false;
}

bool ReactNativeRootView::get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const {
	const Ref<RNShadowNode> node = registry.get_shadow_node(p_tag);
	if (node.is_null() || !_find_page_position(committed_tree, p_tag, Point2(), r_page_position)) {
		return false;
	}
	r_local_rect = node->layout;
	return true;
}

void ReactNativeRootView::_on_focus_entered(int p_tag, ObjectID p_control_id) {
	if (registry.get_tag(p_control_id) != p_tag) {
		return;
	}
	_set_focused_tag(p_tag);
}

void ReactNativeRootView::_on_focus_exited(int p_tag, ObjectID p_control_id) {
	if (registry.get_tag(p_control_id) != p_tag) {
		return;
	}
	if (focused_tag == p_tag) {
		_set_focused_tag(0);
	}
}

void ReactNativeRootView::_set_focused_tag(int p_tag) {
	if (focused_tag == p_tag || !ui_manager) {
		return;
	}
	Vector<RNNativeEvent> events;
	if (focused_tag != 0) {
		RNNativeEvent blur;
		blur.tag = focused_tag;
		blur.name = "topBlur";
		blur.priority = FabricUIManager::EVENT_PRIORITY_DISCRETE;
		blur.generation = ui_manager->get_runtime_generation();
		blur.payload["target"] = focused_tag;
		events.push_back(blur);
	}
	focused_tag = p_tag;
	if (focused_tag != 0) {
		RNNativeEvent focus;
		focus.tag = focused_tag;
		focus.name = "topFocus";
		focus.priority = FabricUIManager::EVENT_PRIORITY_DISCRETE;
		focus.generation = ui_manager->get_runtime_generation();
		focus.payload["target"] = focused_tag;
		events.push_back(focus);
	}
	_enqueue_events(events);
}

void ReactNativeRootView::input(const Ref<InputEvent> &p_event) {
	_route_input(p_event);
}

void ReactNativeRootView::_route_input(const Ref<InputEvent> &p_event) {
	if (!ui_manager || committed_tree.is_null()) {
		return;
	}
	const uint64_t generation = ui_manager->get_runtime_generation();
	RNInputRouter::RouteResult result;
	if (Ref<InputEventKey> key = p_event; key.is_valid()) {
		Control *owner = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
		const int target = owner ? registry.get_tag(owner->get_instance_id()) : 0;
		result = input_router.route_key(key, target, generation);
	} else {
		Point2 screen_position;
		if (Ref<InputEventMouse> mouse = p_event; mouse.is_valid()) {
			screen_position = mouse->get_global_position();
		} else if (Ref<InputEventScreenTouch> touch = p_event; touch.is_valid()) {
			screen_position = touch->get_position();
		} else if (Ref<InputEventScreenDrag> drag = p_event; drag.is_valid()) {
			screen_position = drag->get_position();
		} else {
			return;
		}
		const Point2 root_position = get_global_transform_with_canvas().affine_inverse().xform(screen_position);
		result = input_router.route_pointer(p_event, committed_tree, registry, get_size(), root_tag, generation, root_position, screen_position);
	}

	_enqueue_events(result.events, false);
	if (result.focus_tag != 0) {
		Control *control = Object::cast_to<Control>(registry.get_node(result.focus_tag));
		if (control && control->get_focus_mode() != Control::FOCUS_NONE) {
			_set_focused_tag(result.focus_tag);
			control->grab_focus();
		} else {
			// Pressing something unfocusable drops focus, as a browser does. Godot's GUI
			// would normally do this, but it never sees the press: we accept it here.
			if (Control *focused = Object::cast_to<Control>(registry.get_node(focused_tag))) {
				focused->release_focus();
			}
			_set_focused_tag(0);
		}
	}
	if (!result.events.is_empty()) {
		ui_manager->request_event_flush();
	}
	if (result.accepted) {
		accept_event();
	}
}
