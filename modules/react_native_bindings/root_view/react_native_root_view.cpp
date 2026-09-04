#include "react_native_root_view.h"

#include "../fabric/fabric_ui_manager.h"
#include "../fabric/rn_layout.h"
#include "../fabric/rn_view_style.h"

#include "core/error/error_macros.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/main/viewport.h"

#include <cmath>

namespace {
float number_prop(const Dictionary &p_props, const String &p_name, float p_default = 0.0f) {
	const Variant value = p_props.get(p_name, p_default);
	return value.get_type() == Variant::INT || value.get_type() == Variant::FLOAT ? float(value) : p_default;
}

Vector4 border_widths(const Dictionary &p_props) {
	const float all = number_prop(p_props, "borderWidth");
	return Vector4(number_prop(p_props, "borderTopWidth", all), number_prop(p_props, "borderRightWidth", all), number_prop(p_props, "borderBottomWidth", all), number_prop(p_props, "borderLeftWidth", all));
}
} // namespace

ReactNativeRootView::ReactNativeRootView() {
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
	registry.clear();
}

void ReactNativeRootView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("mount", "tree"), &ReactNativeRootView::mount);
	ClassDB::bind_method(D_METHOD("get_root_tag"), &ReactNativeRootView::get_root_tag);
	ClassDB::bind_method(D_METHOD("set_application_key", "application_key"), &ReactNativeRootView::set_application_key);
	ClassDB::bind_method(D_METHOD("get_application_key"), &ReactNativeRootView::get_application_key);
	ClassDB::bind_method(D_METHOD("reload"), &ReactNativeRootView::reload);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "application_key"), "set_application_key", "get_application_key");
}

void ReactNativeRootView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!registered) {
				if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
					registered = true;
					coordinator->register_root(this);
				}
			}
		} break;
		case NOTIFICATION_RESIZED: {
			if (committed_tree.is_valid()) {
				_layout_and_mount(mounted_revision);
			}
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			_publish_transform_snapshot();
		} break;
		case NOTIFICATION_EXIT_TREE: {
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

void ReactNativeRootView::_attach_surface(const RNSurfaceRoute &p_route) {
	root_tag = p_route.root_tag;
	runtime_generation = p_route.runtime_generation;
	surface_epoch = p_route.surface_epoch;
	mounted_revision = 0;
}

Vector<RNNativeEvent> ReactNativeRootView::_prepare_surface_stop() {
	Vector<RNNativeEvent> events = input_router.cancel_all(committed_tree, root_tag, runtime_generation);
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
	_clear_scene_state();
	root_tag = 0;
	runtime_generation = 0;
	surface_epoch = 0;
	mounted_revision = 0;
}

void ReactNativeRootView::_clear_scene_state() {
	input_router.clear();
	committed_tree.unref();
	registry.clear();
	layout_cache.clear();
	direct_prop_overrides.clear();
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
	if (!is_inside_tree() || p_commit.runtime_generation != runtime_generation || p_commit.root_tag != root_tag || p_commit.surface_epoch != surface_epoch) {
		return;
	}
	if (!RNShadowNode::is_within_depth_limit(p_commit.tree)) {
		if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
			coordinator->fail_surface(root_tag, surface_epoch, vformat("Shadow tree exceeds depth limit %d.", RNShadowNode::MAX_DEPTH));
		}
		return;
	}
	committed_tree = p_commit.tree;
	_apply_declarative_overrides(committed_tree);
	if (!_layout_and_mount(p_commit.revision)) {
		if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
			coordinator->fail_surface(root_tag, surface_epoch, "Native staging or mount failed.");
		}
		return;
	}
	mounted_revision = p_commit.revision;
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		coordinator->mark_surface_mounted(root_tag, surface_epoch, mounted_revision);
	}
}

void ReactNativeRootView::_apply_declarative_overrides(const Ref<RNShadowNode> &p_node) {
	if (p_node.is_null()) {
		return;
	}
	Dictionary *overrides = direct_prop_overrides.getptr(p_node->tag);
	if (overrides) {
		for (const String &key : p_node->declarative_prop_keys) {
			overrides->erase(key);
		}
		if (overrides->is_empty()) {
			direct_prop_overrides.erase(p_node->tag);
		} else {
			const Array keys = overrides->keys();
			for (int i = 0; i < keys.size(); ++i) {
				p_node->props[keys[i]] = (*overrides)[keys[i]];
			}
		}
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		_apply_declarative_overrides(child);
	}
}

bool ReactNativeRootView::_layout_and_mount(uint64_t p_revision) {
	if (committed_tree.is_null()) {
		return false;
	}
	RNLayout::calculate(committed_tree, get_size());

	Control *staging = memnew(Control);
	staging->set_size(get_size());
	RNRegistry staged_registry;
	_build_node(committed_tree, staging, staged_registry);

	Vector<Node *> previous;
	for (int i = 0; i < get_child_count(); ++i) {
		previous.push_back(get_child(i));
	}
	Vector<Node *> replacement;
	while (staging->get_child_count() > 0) {
		Node *child = staging->get_child(0);
		staging->remove_child(child);
		replacement.push_back(child);
	}
	memdelete(staging);
	replacing_tree = true;
	for (Node *child : previous) {
		remove_child(child);
	}
	for (Node *child : replacement) {
		add_child(child);
	}
	for (Node *child : previous) {
		child->queue_free();
	}
	registry = staged_registry;

	Vector<RNNativeEvent> events = input_router.reconcile_tree(committed_tree, registry, root_tag, runtime_generation);
	HashMap<int, Rect2> next_layout_cache;
	_queue_layout_events(committed_tree, next_layout_cache, events);
	layout_cache = next_layout_cache;
	_enqueue_events(events);

	if (focused_tag != 0) {
		Control *focused = Object::cast_to<Control>(registry.get_node(focused_tag));
		if (focused && focused->get_focus_mode() != Control::FOCUS_NONE) {
			focused->grab_focus();
		} else {
			_set_focused_tag(0);
		}
	}
	replacing_tree = false;

	auto snapshot = std::make_shared<RNSurfaceSnapshot>();
	snapshot->root_tag = root_tag;
	snapshot->runtime_generation = runtime_generation;
	snapshot->surface_epoch = surface_epoch;
	snapshot->revision = p_revision;
	_build_snapshot_node(committed_tree, 0, Point2(), get_global_transform_with_canvas(), registry, *snapshot);
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		coordinator->publish_snapshot(snapshot);
	}
	return true;
}

Control *ReactNativeRootView::_build_node(const Ref<RNShadowNode> &p_node, Control *p_parent, RNRegistry &r_registry, bool p_branch_targetable) {
	if (p_node.is_null() || p_node->view_name == "RCTRawText") {
		return nullptr;
	}
	if (p_node->view_name == "RCTRootView") {
		for (const Ref<RNShadowNode> &child : p_node->children) {
			_build_node(child, p_parent, r_registry, p_branch_targetable);
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
		r_registry.register_node(p_node->tag, label, p_node);
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
	r_registry.register_node(p_node->tag, panel, p_node);
	panel->connect("focus_entered", callable_mp(this, &ReactNativeRootView::_on_focus_entered).bind(p_node->tag, panel->get_instance_id()));
	panel->connect("focus_exited", callable_mp(this, &ReactNativeRootView::_on_focus_exited).bind(p_node->tag, panel->get_instance_id()));
	for (const Ref<RNShadowNode> &child : p_node->children) {
		_build_node(child, panel, r_registry, descendants_targetable);
	}
	return panel;
}

void ReactNativeRootView::_queue_layout_events(const Ref<RNShadowNode> &p_node, HashMap<int, Rect2> &r_next_cache, Vector<RNNativeEvent> &r_events) {
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
			RNNativeEvent event;
			event.tag = p_node->tag;
			event.name = "topLayout";
			event.priority = FabricUIManager::EVENT_PRIORITY_DEFAULT;
			event.payload["layout"] = rectangle;
			r_events.push_back(event);
		}
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		_queue_layout_events(child, r_next_cache, r_events);
	}
}

void ReactNativeRootView::_build_snapshot_node(const Ref<RNShadowNode> &p_node, int p_parent_tag, const Point2 &p_parent_position, const Transform2D &p_window_transform, const RNRegistry &p_registry, RNSurfaceSnapshot &r_snapshot) {
	if (p_node.is_null()) {
		return;
	}
	RNMountedNodeSnapshot node;
	node.tag = p_node->tag;
	node.parent_tag = p_parent_tag;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (child.is_valid()) {
			node.child_tags.push_back(child->tag);
		}
	}
	node.view_name = p_node->view_name;
	node.native_id = String(p_node->props.get("nativeID", String()));
	node.text_content = p_node->collect_text();
	node.local_rect = p_node->layout;
	node.root_rect = Rect2(p_parent_position + p_node->layout.position, p_node->layout.size);
	node.window_rect = p_window_transform.xform(node.root_rect);
	node.border_width = border_widths(p_node->props);
	node.inner_size = Size2(MAX(0.0f, node.root_rect.size.x - node.border_width.y - node.border_width.w), MAX(0.0f, node.root_rect.size.y - node.border_width.x - node.border_width.z));
	node.offset_parent_tag = p_parent_tag;
	node.offset = p_node->layout.position;
	if (const RNMountedNodeSnapshot *parent = r_snapshot.nodes.getptr(p_parent_tag)) {
		node.offset -= Point2(parent->border_width.w, parent->border_width.x);
	}
	if (Node *object = p_registry.get_node(p_node->tag)) {
		node.object_id = object->get_instance_id();
	}
	node.shadow_node = p_node;
	r_snapshot.nodes[p_node->tag] = node;
	if (!node.native_id.is_empty()) {
		r_snapshot.native_id_index[node.native_id].push_back(node.tag);
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		_build_snapshot_node(child, p_node->tag, node.root_rect.position, p_window_transform, p_registry, r_snapshot);
	}
}

void ReactNativeRootView::_publish_transform_snapshot() {
	if (root_tag == 0 || committed_tree.is_null()) {
		return;
	}
	ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton();
	if (!coordinator) {
		return;
	}
	std::shared_ptr<const RNSurfaceSnapshot> previous = coordinator->get_snapshot(root_tag);
	if (!previous || previous->surface_epoch != surface_epoch) {
		return;
	}
	auto replacement = std::make_shared<RNSurfaceSnapshot>(*previous);
	const Transform2D transform = get_global_transform_with_canvas();
	for (KeyValue<int, RNMountedNodeSnapshot> &entry : replacement->nodes) {
		entry.value.window_rect = transform.xform(entry.value.root_rect);
	}
	coordinator->publish_snapshot(replacement);
}

bool ReactNativeRootView::get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const {
	ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton();
	std::shared_ptr<const RNSurfaceSnapshot> snapshot = coordinator ? coordinator->get_snapshot(root_tag) : nullptr;
	const RNMountedNodeSnapshot *node = snapshot ? snapshot->nodes.getptr(p_tag) : nullptr;
	if (!node) {
		return false;
	}
	r_local_rect = node->local_rect;
	r_page_position = node->root_rect.position;
	return true;
}

void ReactNativeRootView::_apply_imperative(const RNImperativeRequest &p_request) {
	if (p_request.runtime_generation != runtime_generation || p_request.root_tag != root_tag || p_request.surface_epoch != surface_epoch) {
		return;
	}
	Ref<RNShadowNode> node = registry.get_shadow_node(p_request.tag);
	if (node.is_null() || node->view_name != p_request.component_name) {
		return;
	}
	if (p_request.kind == RNImperativeRequestKind::COMMAND) {
		Control *control = Object::cast_to<Control>(registry.get_node(p_request.tag));
		if (!control) {
			return;
		}
		if (p_request.component_name == "RCTView" && p_request.command_name == "focus") {
			control->grab_focus();
		} else if (p_request.component_name == "RCTView" && p_request.command_name == "blur") {
			control->release_focus();
		} else if (p_request.command_name == "hotspotUpdate" || p_request.command_name == "setPressed") {
			WARN_PRINT_ONCE(vformat("View command %s is not supported by the Godot host.", p_request.command_name));
		} else {
			WARN_PRINT(vformat("%s command %s is not supported by the Godot host.", p_request.component_name, p_request.command_name));
		}
		return;
	}
	if (p_request.payload.get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary &overrides = direct_prop_overrides[p_request.tag];
	const Dictionary props = p_request.payload;
	const Array keys = props.keys();
	for (int i = 0; i < keys.size(); ++i) {
		if (props[keys[i]].get_type() == Variant::NIL) {
			overrides.erase(keys[i]);
			node->props.erase(keys[i]);
		} else {
			overrides[keys[i]] = props[keys[i]];
			node->props[keys[i]] = props[keys[i]];
		}
	}
	_layout_and_mount(mounted_revision);
}

void ReactNativeRootView::_on_focus_entered(int p_tag, ObjectID p_control_id) {
	if (replacing_tree) {
		return;
	}
	if (registry.get_tag(p_control_id) == p_tag) {
		_set_focused_tag(p_tag);
	}
}

void ReactNativeRootView::_on_focus_exited(int p_tag, ObjectID p_control_id) {
	if (replacing_tree) {
		return;
	}
	if (registry.get_tag(p_control_id) == p_tag && focused_tag == p_tag) {
		_set_focused_tag(0);
	}
}

void ReactNativeRootView::_set_focused_tag(int p_tag) {
	if (focused_tag == p_tag) {
		return;
	}
	Vector<RNNativeEvent> events;
	if (focused_tag != 0) {
		RNNativeEvent blur;
		blur.tag = focused_tag;
		blur.name = "topBlur";
		blur.priority = FabricUIManager::EVENT_PRIORITY_DISCRETE;
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

void ReactNativeRootView::input(const Ref<InputEvent> &p_event) {
	_route_input(p_event);
}

void ReactNativeRootView::_route_input(const Ref<InputEvent> &p_event) {
	if (root_tag == 0 || committed_tree.is_null()) {
		return;
	}
	RNInputRouter::RouteResult result;
	if (Ref<InputEventKey> key = p_event; key.is_valid()) {
		Control *owner = get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
		result = input_router.route_key(key, owner ? registry.get_tag(owner->get_instance_id()) : 0, runtime_generation);
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
		result = input_router.route_pointer(p_event, committed_tree, registry, get_size(), root_tag, runtime_generation, root_position, screen_position);
	}
	_enqueue_events(result.events);
	if (result.focus_tag != 0) {
		Control *control = Object::cast_to<Control>(registry.get_node(result.focus_tag));
		if (control && control->get_focus_mode() != Control::FOCUS_NONE) {
			control->grab_focus();
		} else if (Control *focused = Object::cast_to<Control>(registry.get_node(focused_tag))) {
			focused->release_focus();
		}
	}
	if (result.accepted) {
		accept_event();
	}
}
