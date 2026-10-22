#include "react_native_root_view.h"

#include "../components/rn_clip.h"
#include "../host/react_native_host.h"
#include "../turbomodules/rn_godot_alerts.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"

using namespace facebook::react;

ReactNativeRootView::ReactNativeRootView() {
	container = memnew(Control);
	container->set_name("ReactNativeMountContainer");
	container->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	container->set_mouse_filter(MOUSE_FILTER_PASS);
	add_child(container, false, INTERNAL_MODE_FRONT);
	set_mouse_filter(MOUSE_FILTER_PASS);
	set_process_input(true);
	set_notify_transform(true);
}

ReactNativeRootView::~ReactNativeRootView() {
	_disconnect_scene_target();
	_detach_surface();
}

void ReactNativeRootView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_alert_handler", "handler"), &ReactNativeRootView::set_alert_handler);
	ClassDB::bind_method(D_METHOD("complete_alert", "request", "result"), &ReactNativeRootView::complete_alert);
	ClassDB::bind_method(D_METHOD("attach_scene_binding", "target", "binding"), &ReactNativeRootView::attach_scene_binding);
	ClassDB::bind_method(D_METHOD("detach_scene_binding"), &ReactNativeRootView::detach_scene_binding);
	ClassDB::bind_method(D_METHOD("get_root_tag"), &ReactNativeRootView::get_root_tag);
	ClassDB::bind_method(D_METHOD("set_application_key", "application_key"), &ReactNativeRootView::set_application_key);
	ClassDB::bind_method(D_METHOD("get_application_key"), &ReactNativeRootView::get_application_key);
	ClassDB::bind_method(D_METHOD("reload"), &ReactNativeRootView::reload);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "application_key"), "set_application_key", "get_application_key");
}

void ReactNativeRootView::_notification(int p_what) {
	ReactNativeHost *host = ReactNativeHost::get_singleton();
	if (!host) {
		return;
	}
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ReactNativeRootView::_surface_changed));
			host->register_root(this);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &ReactNativeRootView::_surface_changed));
			host->unregister_root(this);
		} break;
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			_surface_changed();
		} break;
	}
}

// Notifications arrive while Godot propagates them through the tree, where
// measuring the theme cannot add its probe controls, so updates run deferred
// and at most once per frame.
void ReactNativeRootView::_surface_changed() {
	if (surface_id != 0 && !layout_pending) {
		layout_pending = true;
		callable_mp(this, &ReactNativeRootView::_update_layout).call_deferred();
	}
}

void ReactNativeRootView::_update_layout() {
	layout_pending = false;
	if (surface_id != 0 && ReactNativeHost::get_singleton()) {
		ReactNativeHost::get_singleton()->update_root_layout(this);
	}
}

void ReactNativeRootView::set_application_key(const String &p_key) {
	ERR_FAIL_COND_MSG(p_key.is_empty(), "ReactNativeRootView.application_key cannot be empty.");
	if (application_key == p_key) {
		return;
	}
	application_key = p_key;
	reload();
}

void ReactNativeRootView::reload() {
	if (is_inside_tree() && ReactNativeHost::get_singleton()) {
		ReactNativeHost::get_singleton()->restart_root(this);
	}
}

RNMountedSurface ReactNativeRootView::_attach_surface(SurfaceId p_surface, std::shared_ptr<const RNComponentViewFactories> p_factories) {
	_detach_surface();
	surface_id = p_surface;
	root_component = std::make_unique<RNRootComponentView>(p_surface, container);
	views = std::make_unique<RNComponentViewRegistry>(std::move(p_factories));
	views->register_root(p_surface, root_component.get());
	views->set_change_listener([this](RNComponentView *p_view) { _connect_input(p_view); });
	views->set_destroy_listener([this](Tag p_tag) { input_router.view_destroyed(p_tag); });
	return { views.get(), nullptr };
}

void ReactNativeRootView::_detach_surface() {
	if (!views) {
		return;
	}
	input_router.cancel_all();
	views->clear();
	views.reset();
	root_component.reset();
	surface_id = 0;
}

void ReactNativeRootView::_connect_control(Control *p_control) {
	const Callable observer = callable_mp(this, &ReactNativeRootView::_on_host_gui_input).bind(p_control->get_instance_id());
	if (!p_control->is_connected(SceneStringName(gui_input), observer)) {
		p_control->connect(SceneStringName(gui_input), observer);
	}
}

void ReactNativeRootView::_connect_input(RNComponentView *p_view) {
	_connect_control(p_view->get_control());
	for (Control *control : p_view->get_input_controls()) {
		_connect_control(control);
	}
}

void ReactNativeRootView::input(const Ref<InputEvent> &p_event) {
	_begin_input(p_event, get_viewport()->get_instance_id());
}

// Godot delivers gui_input to the deepest control first and then bubbles it
// through PASS parents. The first React control that sees an input serial
// claims it; motion nobody claims still moves hover off React targets.
void ReactNativeRootView::_begin_input(const Ref<InputEvent> &p_event, ObjectID p_viewport_id) {
	++input_serial;
	if (Ref<InputEventMouseMotion>(p_event).is_valid()) {
		callable_mp(this, &ReactNativeRootView::_route_unclaimed_motion).call_deferred(p_event, input_serial, p_viewport_id);
	} else if (Ref<InputEventKey> key = p_event; key.is_valid() && views && get_viewport()) {
		Control *focus_owner = get_viewport()->gui_get_focus_owner();
		if (focus_owner && container->is_ancestor_of(focus_owner)) {
			RNComponentView *view = views->find_for_control(focus_owner);
			input_router.route_key(key, RNInputRouter::target_for(view, Point2()));
		}
	}
}

void ReactNativeRootView::_route_unclaimed_motion(const Ref<InputEvent> &p_event, uint64_t p_serial, ObjectID p_viewport_id) {
	Viewport *viewport = Object::cast_to<Viewport>(ObjectDB::get_instance(p_viewport_id));
	if (viewport && claimed_input_serial != p_serial) {
		_route(p_event, nullptr, viewport);
	}
}

void ReactNativeRootView::_on_host_gui_input(const Ref<InputEvent> &p_event, ObjectID p_control_id) {
	Control *control = Object::cast_to<Control>(ObjectDB::get_instance(p_control_id));
	if (!control || !views || claimed_input_serial == input_serial || Ref<InputEventKey>(p_event).is_valid()) {
		return;
	}
	RNComponentView *view = views->find_for_control(control);
	if (Ref<InputEventMouse> mouse = p_event; mouse.is_valid() && view) {
		view = _view_inside_clips(view, control->get_global_transform().xform(mouse->get_position()));
	}
	if (view == root_component.get()) {
		view = nullptr;
	}
	if (!view && Ref<InputEventMouseMotion>(p_event).is_null()) {
		return;
	}
	claimed_input_serial = input_serial;
	_route(p_event->xformed_by(control->get_global_transform_with_canvas()), view, control->get_viewport());
}

// Godot picks controls by rectangle; a point outside a rounded clip belongs to
// the view that owns the clip.
RNComponentView *ReactNativeRootView::_view_inside_clips(RNComponentView *p_view, const Point2 &p_canvas_point) const {
	Node *node = p_view->get_control();
	while (node && node != container && !Object::cast_to<Viewport>(node)) {
		Control *control = Object::cast_to<Control>(node);
		if (control && control->has_meta(RNComponentView::TAG_META) && rn_clip_contains(rn_clip_stack(control), p_canvas_point)) {
			return control == p_view->get_control() ? p_view : views->find_for_control(control);
		}
		node = node->get_parent();
	}
	return p_view;
}

void ReactNativeRootView::_route(const Ref<InputEvent> &p_event, RNComponentView *p_view, Viewport *p_viewport) {
	Point2 viewport_position;
	if (Ref<InputEventMouse> mouse = p_event; mouse.is_valid()) {
		viewport_position = mouse->get_position();
	} else if (Ref<InputEventScreenTouch> touch = p_event; touch.is_valid()) {
		viewport_position = touch->get_position();
	} else if (Ref<InputEventScreenDrag> drag = p_event; drag.is_valid()) {
		viewport_position = drag->get_position();
	} else {
		return;
	}
	const Transform2D root_transform = get_screen_transform();
	if (!views || Math::is_zero_approx(root_transform.determinant())) {
		return;
	}
	RNInputRouter::Position position;
	position.viewport = viewport_position;
	position.screen = p_viewport->get_screen_transform().xform(viewport_position);
	position.page = root_transform.affine_inverse().xform(position.screen);
	const Point2 local = p_view ? p_view->get_control()->get_global_transform_with_canvas().affine_inverse().xform(viewport_position) : Point2();
	Control *pressed = input_router.route_pointer(p_event, RNInputRouter::target_for(p_view, local), position);
	if (Ref<InputEventMouseButton> button = p_event; button.is_valid() && button->is_pressed()) {
		callable_mp(this, &ReactNativeRootView::_cancel_mouse_taken_by_popup).call_deferred(p_viewport->get_instance_id());
	}
	if (!pressed) {
		return;
	}
	if (Control *focus = _focusable_at(pressed, viewport_position)) {
		focus->grab_focus();
	} else if (Control *owner = get_viewport()->gui_get_focus_owner(); owner && container->is_ancestor_of(owner)) {
		owner->release_focus();
	}
}

// A press can open a popup, such as a context menu, that then receives the
// release instead of the pressed view.
void ReactNativeRootView::_cancel_mouse_taken_by_popup(ObjectID p_viewport_id) {
	Viewport *viewport = Object::cast_to<Viewport>(ObjectDB::get_instance(p_viewport_id));
	while (viewport && !viewport->is_embedding_subwindows()) {
		Window *embedded = Object::cast_to<Window>(viewport);
		viewport = embedded ? embedded->get_embedder() : nullptr;
	}
	if (!viewport || !input_router.is_mouse_pressed()) {
		return;
	}
	const TypedArray<Window> windows = viewport->get_embedded_subwindows();
	for (int i = 0; i < windows.size(); ++i) {
		const Window *popup = Object::cast_to<Window>(windows[i]);
		if (popup && popup->is_visible() && popup->get_flag(Window::FLAG_POPUP) && popup->has_focus()) {
			input_router.cancel_mouse();
			return;
		}
	}
}

// Flattening can hoist a pressed view next to the focusable view that
// logically contains it, so earlier siblings under the pointer count as
// ancestors too.
Control *ReactNativeRootView::_focusable_at(Control *p_pressed, const Point2 &p_viewport_position) const {
	auto focusable = [&p_viewport_position](Control *p_control) {
		const Point2 local = p_control->get_global_transform_with_canvas().affine_inverse().xform(p_viewport_position);
		return p_control->is_visible_in_tree() && (p_control->get_focus_mode() == FOCUS_CLICK || p_control->get_focus_mode() == FOCUS_ALL) && Rect2(Point2(), p_control->get_size()).has_point(local);
	};
	for (Node *node = p_pressed; node && node != container; node = node->get_parent()) {
		Control *control = Object::cast_to<Control>(node);
		if (control && focusable(control)) {
			return control;
		}
		for (int i = node->get_index() - 1; i >= 0; --i) {
			Control *sibling = Object::cast_to<Control>(node->get_parent()->get_child(i));
			if (sibling && sibling->has_meta(RNComponentView::TAG_META) && focusable(sibling)) {
				return sibling;
			}
		}
	}
	return nullptr;
}

Dictionary ReactNativeRootView::attach_scene_binding(Object *p_target, const Ref<RNSceneBinding> &p_binding) {
	RNError error;
	auto attachment = std::make_shared<RNSceneAttachment>();
	if (p_binding.is_null() || !p_binding->compile(p_target, *attachment, error)) {
		if (!error.is_set()) {
			error = RNError::make(RNErrorCode::VALIDATION, "binding resource is required", "scene.attach");
		}
		return error.to_dictionary();
	}
	attachment->identity = next_binding_identity++;
	_disconnect_scene_target();
	scene_attachment = attachment;
	Node *node = Object::cast_to<Node>(p_target);
	scene_target_available = !node || node->is_inside_tree();
	if (node) {
		node->connect("tree_entered", callable_mp(this, &ReactNativeRootView::_scene_target_changed).bind(true));
		node->connect("tree_exiting", callable_mp(this, &ReactNativeRootView::_scene_target_changed).bind(false));
	}
	_scene_target_changed(scene_target_available);
	return Dictionary();
}

void ReactNativeRootView::detach_scene_binding() {
	_disconnect_scene_target();
	scene_attachment.reset();
	_scene_target_changed(false);
}

bool ReactNativeRootView::complete_alert(const String &p_request, const Dictionary &p_result) {
	RNError error;
	return GodotAlerts::get_singleton() && GodotAlerts::get_singleton()->complete_for(get_instance_id(), p_request, p_result, error);
}

void ReactNativeRootView::_disconnect_scene_target() {
	Node *node = scene_attachment ? Object::cast_to<Node>(ObjectDB::get_instance(scene_attachment->target)) : nullptr;
	if (!node) {
		return;
	}
	for (const auto &entry : { std::pair<const char *, bool>("tree_entered", true), { "tree_exiting", false } }) {
		const Callable callback = callable_mp(this, &ReactNativeRootView::_scene_target_changed).bind(entry.second);
		if (node->is_connected(entry.first, callback)) {
			node->disconnect(entry.first, callback);
		}
	}
}

void ReactNativeRootView::_scene_target_changed(bool p_available) {
	scene_target_available = p_available;
	if (surface_id != 0 && ReactNativeHost::get_singleton()) {
		ReactNativeHost::get_singleton()->scene_binding_changed(surface_id);
	}
}
