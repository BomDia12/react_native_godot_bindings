#include "react_native_root_view.h"

#include "../components/rn_clip.h"
#include "../editor/rn_contract_codegen.h"
#include "../host/react_native_host.h"
#include "../turbomodules/rn_godot_alerts.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/core_string_names.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/font.h"

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
	connect(CoreStringName(script_changed), callable_mp(this, &ReactNativeRootView::_apply_contract));
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
	ClassDB::bind_method(D_METHOD("set_initial_properties", "properties"), &ReactNativeRootView::set_initial_properties);
	ClassDB::bind_method(D_METHOD("get_initial_properties"), &ReactNativeRootView::get_initial_properties);
	ClassDB::bind_method(D_METHOD("set_fit_content", "fit"), &ReactNativeRootView::set_fit_content);
	ClassDB::bind_method(D_METHOD("get_fit_content"), &ReactNativeRootView::get_fit_content);
	ClassDB::bind_method(D_METHOD("set_editor_preview", "preview"), &ReactNativeRootView::set_editor_preview);
	ClassDB::bind_method(D_METHOD("is_editor_preview"), &ReactNativeRootView::is_editor_preview);
	ClassDB::bind_method(D_METHOD("get_native_node", "native_id"), &ReactNativeRootView::get_native_node);
	ClassDB::bind_method(D_METHOD("post_message", "name", "payload"), &ReactNativeRootView::post_message, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("set_contract_values", "values"), &ReactNativeRootView::set_contract_values);
	ClassDB::bind_method(D_METHOD("get_contract_values"), &ReactNativeRootView::get_contract_values);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "application_key"), "set_application_key", "get_application_key");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "initial_properties"), "set_initial_properties", "get_initial_properties");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fit_content", PROPERTY_HINT_ENUM, "None,Width,Height,Both"), "set_fit_content", "get_fit_content");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_preview"), "set_editor_preview", "is_editor_preview");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "contract_values", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_contract_values", "get_contract_values");
	ADD_SIGNAL(MethodInfo("contract_changed", PropertyInfo(Variant::DICTIONARY, "values")));
	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::NIL, "payload", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("scene_loaded", PropertyInfo(Variant::STRING, "native_id"), PropertyInfo(Variant::OBJECT, "instance", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	BIND_ENUM_CONSTANT(FIT_CONTENT_NONE);
	BIND_ENUM_CONSTANT(FIT_CONTENT_WIDTH);
	BIND_ENUM_CONSTANT(FIT_CONTENT_HEIGHT);
	BIND_ENUM_CONSTANT(FIT_CONTENT_BOTH);
}

void ReactNativeRootView::_notification(int p_what) {
	ReactNativeHost *host = ReactNativeHost::get_singleton();
	if (!host) {
		return;
	}
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (contract_resource.is_null()) {
				_load_contract();
			}
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ReactNativeRootView::_surface_changed));
			_connect_window(true);
			reported_state = get_state();
			if (_is_running()) {
				host->register_root(this);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &ReactNativeRootView::_surface_changed));
			_connect_window(false);
			host->unregister_root(this);
		} break;
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_WM_DPI_CHANGE: {
			_surface_changed();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED:
		case NOTIFICATION_PAUSED:
		case NOTIFICATION_UNPAUSED:
		case NOTIFICATION_DISABLED:
		case NOTIFICATION_ENABLED: {
			_state_changed();
		} break;
		case NOTIFICATION_DRAW: {
			if (!_is_running()) {
				_draw_placeholder();
			}
		} break;
	}
}

// Window focus and viewport resizes don't reach controls as notifications.
void ReactNativeRootView::_connect_window(bool p_connect) {
	Window *own_window = get_window();
	const Callable state_changed = callable_mp(this, &ReactNativeRootView::_state_changed);
	const std::pair<Object *, std::pair<const char *, Callable>> signals[] = {
		{ own_window, { "focus_entered", state_changed } },
		{ own_window, { "focus_exited", state_changed } },
		{ get_viewport(), { "size_changed", callable_mp(this, &ReactNativeRootView::_surface_changed) } },
	};
	for (const auto &[object, connection] : signals) {
		if (p_connect) {
			object->connect(connection.first, connection.second);
		} else {
			object->disconnect(connection.first, connection.second);
		}
	}
}

// A root the editor doesn't preview leaves the runtime alone and only shows
// which component it would host, as do roots in an editor that only
// generates contracts.
bool ReactNativeRootView::_is_running() const {
	return (editor_preview || !Engine::get_singleton()->is_editor_hint()) && !rn_is_generating_contracts();
}

void ReactNativeRootView::_draw_placeholder() {
	const Rect2 rect(Point2(), get_size());
	draw_rect(rect, ::Color(1, 1, 1, 0.3), false, 1);
	const Ref<Font> font = get_theme_default_font();
	const int font_size = get_theme_default_font_size();
	const Size2 text_size = font->get_string_size(application_key, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
	draw_string(font, (rect.size - text_size) / 2 + Vector2(0, font->get_ascent(font_size)), application_key, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, ::Color(1, 1, 1, 0.6));
}

void ReactNativeRootView::_state_changed() {
	const RNRootState state = get_state();
	if (state == reported_state || !is_inside_tree()) {
		return;
	}
	reported_state = state;
	if (ReactNativeHost::get_singleton()) {
		ReactNativeHost::get_singleton()->root_state_changed(this);
	}
}

RNRootState ReactNativeRootView::get_state() const {
	const Window *own_window = is_inside_tree() ? get_window() : nullptr;
	return { .visible = is_visible_in_tree(), .processing = is_inside_tree() && can_process(), .focused = own_window && own_window->has_focus() };
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

void ReactNativeRootView::_content_size_changed(const Size2 &p_size) {
	if (content_size != p_size) {
		content_size = p_size;
		update_minimum_size();
	}
}

Size2 ReactNativeRootView::get_minimum_size() const {
	return Size2(fits_width() ? content_size.x : 0, fits_height() ? content_size.y : 0);
}

void ReactNativeRootView::set_fit_content(FitContent p_fit) {
	if (fit_content == p_fit) {
		return;
	}
	fit_content = p_fit;
	update_minimum_size();
	_surface_changed();
}

void ReactNativeRootView::set_initial_properties(const Dictionary &p_properties) {
	if (initial_properties == p_properties) {
		return;
	}
	initial_properties = p_properties.duplicate(true);
	reload();
}

void ReactNativeRootView::set_editor_preview(bool p_preview) {
	if (editor_preview == p_preview) {
		return;
	}
	editor_preview = p_preview;
	queue_redraw();
	ReactNativeHost *host = ReactNativeHost::get_singleton();
	if (!is_inside_tree() || !host) {
		return;
	}
	if (_is_running()) {
		host->register_root(this);
	} else {
		host->unregister_root(this);
	}
}

PackedStringArray ReactNativeRootView::get_configuration_warnings() const {
	PackedStringArray warnings = Control::get_configuration_warnings();
	if (!contract_error.is_empty()) {
		warnings.push_back(vformat("The contract of '%s' is invalid: %s", application_key, contract_error));
	}
	if (contract_resource.is_valid() && !rn_generated_contracts_current()) {
		warnings.push_back("The generated contract TypeScript differs from the contracts; save the contract or build the bundle to regenerate it.");
	}
	return warnings;
}

Node *ReactNativeRootView::get_native_node(const String &p_native_id) const {
	RNComponentView *view = views ? views->find_by_native_id(p_native_id.utf8().get_data()) : nullptr;
	return view ? view->get_native_node() : nullptr;
}

void ReactNativeRootView::post_message(const String &p_name, const Variant &p_payload) {
	if (surface_id == 0) {
		WARN_PRINT(vformat("The message '%s' was dropped because the root is not running.", p_name));
		return;
	}
	ReactNativeHost::get_singleton()->send_message(surface_id, p_name, p_payload);
}

void ReactNativeRootView::set_application_key(const String &p_key) {
	ERR_FAIL_COND_MSG(p_key.is_empty(), "ReactNativeRootView.application_key cannot be empty.");
	if (application_key == p_key) {
		return;
	}
	application_key = p_key;
	queue_redraw();
	_load_contract();
	reload();
}

namespace {

PropertyInfo contract_property_info(const RNContractProperty &p_property) {
	Variant::Type type = Variant::NIL;
	switch (p_property.schema.type) {
		case RNValueType::BOOL:
			type = Variant::BOOL;
			break;
		case RNValueType::FLOAT:
			type = Variant::FLOAT;
			break;
		case RNValueType::INTEGER:
		case RNValueType::INT64:
			type = Variant::INT;
			break;
		case RNValueType::STRING:
			type = Variant::STRING;
			break;
		case RNValueType::ARRAY:
			type = Variant::ARRAY;
			break;
		case RNValueType::RECORD:
			type = Variant::DICTIONARY;
			break;
		case RNValueType::BYTES:
			type = Variant::PACKED_BYTE_ARRAY;
			break;
		case RNValueType::COLOR:
			type = Variant::COLOR;
			break;
		case RNValueType::VECTOR2:
			type = Variant::VECTOR2;
			break;
		case RNValueType::VECTOR3:
			type = Variant::VECTOR3;
			break;
		case RNValueType::RECT2:
			type = Variant::RECT2;
			break;
		case RNValueType::TRANSFORM2D:
			type = Variant::TRANSFORM2D;
			break;
		default:
			break;
	}
	if (p_property.schema.nullable) {
		type = Variant::NIL;
	}
	return PropertyInfo(type, p_property.name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | (type == Variant::NIL ? PROPERTY_USAGE_NIL_IS_VARIANT : 0));
}

} // namespace

// The contract named after the application key, from the project's contracts
// folder. Editing the resource applies it again.
void ReactNativeRootView::_load_contract() {
	const Callable reapply = callable_mp(this, &ReactNativeRootView::_apply_contract);
	if (contract_resource.is_valid() && contract_resource->is_connected(CoreStringName(changed), reapply)) {
		contract_resource->disconnect(CoreStringName(changed), reapply);
	}
	contract_resource = rn_load_contract(application_key);
	if (contract_resource.is_valid()) {
		contract_resource->connect(CoreStringName(changed), reapply);
	}
	_apply_contract();
}

// Contract properties become inspector properties, events become signals, and
// the root becomes its own scene binding target, so React receives the values
// and emits the events through GodotScene.
void ReactNativeRootView::_apply_contract() {
	const bool had_contract = contract != nullptr;
	contract.reset();
	RNCompiledContract compiled;
	RNError error;
	contract_error = String();
	if (contract_resource.is_valid()) {
		if (contract_resource->compile(compiled, error) && _contract_free_on_script(compiled, error)) {
			contract = std::make_shared<const RNCompiledContract>(std::move(compiled));
		} else {
			contract_error = error.describe();
			ERR_PRINT(vformat("The contract of '%s' is invalid: %s", application_key, contract_error));
		}
	}
	update_configuration_warnings();
	HashMap<StringName, ::Vector<StringName>> signals;
	for (const RNContractEvent &event : contract ? contract->events : ::Vector<RNContractEvent>()) {
		::Vector<StringName> arguments;
		for (const RNArgumentSchema &argument : event.arguments) {
			arguments.push_back(argument.name);
		}
		signals[event.name] = arguments;
	}
	// A signal whose arguments changed is added again, which drops its connections.
	for (const KeyValue<StringName, ::Vector<StringName>> &signal : contract_signals) {
		const ::Vector<StringName> *arguments = signals.getptr(signal.key);
		if (!arguments || *arguments != signal.value) {
			call(SNAME("remove_user_signal"), signal.key);
		}
	}
	contract_signals = signals;
	if (!contract) {
		if (had_contract) {
			scene_attachment.reset();
			_scene_target_changed(false);
		}
		notify_property_list_changed();
		return;
	}
	_fit_contract_values();
	auto attachment = std::make_shared<RNSceneAttachment>();
	attachment->target = get_instance_id();
	attachment->capability = contract->component;
	attachment->snapshot_method = "get_contract_values";
	attachment->snapshot_schema = contract->snapshot;
	RNRecordFieldSchema values;
	values.name = "values";
	values.value = std::make_shared<RNValueSchema>(contract->snapshot);
	attachment->signals["contract_changed"] = { "changed", { "values" }, RNValueSchema::record({ values }) };
	for (const RNContractEvent &event : contract->events) {
		if (!has_signal(event.name)) {
			Array arguments;
			for (const RNArgumentSchema &argument : event.arguments) {
				arguments.push_back(Dictionary({ { "name", argument.name }, { "type", Variant::NIL } }));
			}
			// Only signals added through the bound method can be removed again.
			call(SNAME("add_user_signal"), event.name, arguments);
		}
		RNSceneCommand command;
		command.method = event.name;
		command.kind = RNSceneCommand::SIGNAL;
		command.schema.name = event.name;
		command.schema.arguments = event.arguments;
		command.schema.mode = RNCallMode::ASYNC;
		attachment->commands[event.name] = command;
	}
	attachment->identity = next_binding_identity++;
	_disconnect_scene_target();
	scene_attachment = attachment;
	_scene_target_changed(true);
	notify_property_list_changed();
}

// A script property or signal with a contract name would take the contract's
// writes or emissions in its place.
bool ReactNativeRootView::_contract_free_on_script(const RNCompiledContract &p_contract, RNError &r_error) const {
	const Ref<Script> script = get_script();
	if (script.is_null()) {
		return true;
	}
	List<PropertyInfo> script_properties;
	script->get_script_property_list(&script_properties);
	for (const RNContractProperty &property : p_contract.properties) {
		for (const PropertyInfo &info : script_properties) {
			if (info.name == property.name) {
				r_error = RNError::make(RNErrorCode::VALIDATION, vformat("property '%s' conflicts with a property of the root's script", property.name), "contract.apply");
				return false;
			}
		}
	}
	for (const RNContractEvent &event : p_contract.events) {
		if (script->has_script_signal(event.name)) {
			r_error = RNError::make(RNErrorCode::VALIDATION, vformat("event '%s' conflicts with a signal of the root's script", event.name), "contract.apply");
			return false;
		}
	}
	return true;
}

// Keeps the values that still match the contract and fills the rest with defaults.
void ReactNativeRootView::_fit_contract_values() {
	Dictionary values;
	for (const RNContractProperty &property : contract->properties) {
		RNError error;
		const Variant current = contract_values.get(property.name, Variant());
		const bool fits = contract_values.has(property.name) && rn_validate_native_value(current, property.schema, error, "value");
		values[property.name] = fits ? current : property.default_value.duplicate(true);
	}
	contract_values = values;
}

void ReactNativeRootView::set_contract_values(const Dictionary &p_values) {
	contract_values = p_values.duplicate(true);
	if (contract) {
		_fit_contract_values();
		_contract_values_changed();
	}
}

// Writes made during one frame reach React as one change at its end.
void ReactNativeRootView::_contract_values_changed() {
	if (!contract_flush_pending) {
		contract_flush_pending = true;
		callable_mp(this, &ReactNativeRootView::_flush_contract).call_deferred();
	}
}

void ReactNativeRootView::_flush_contract() {
	contract_flush_pending = false;
	emit_signal(SNAME("contract_changed"), contract_values);
}

bool ReactNativeRootView::_set(const StringName &p_name, const Variant &p_value) {
	const RNContractProperty *property = contract ? contract->find_property(p_name) : nullptr;
	if (!property) {
		return false;
	}
	RNError error;
	if (!rn_validate_native_value(p_value, property->schema, error, String(p_name))) {
		ERR_PRINT(vformat("%s cannot be set to %s: %s", p_name, p_value, error.describe()));
		return true;
	}
	contract_values[p_name] = p_value;
	_contract_values_changed();
	return true;
}

bool ReactNativeRootView::_get(const StringName &p_name, Variant &r_value) const {
	if (!contract || !contract->find_property(p_name)) {
		return false;
	}
	r_value = contract_values[p_name];
	return true;
}

void ReactNativeRootView::_get_property_list(List<PropertyInfo> *p_list) const {
	if (!contract) {
		return;
	}
	p_list->push_back(PropertyInfo(Variant::NIL, "Component", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP));
	for (const RNContractProperty &property : contract->properties) {
		p_list->push_back(contract_property_info(property));
	}
}

void ReactNativeRootView::reload() {
	if (is_inside_tree() && _is_running() && ReactNativeHost::get_singleton()) {
		ReactNativeHost::get_singleton()->restart_root(this);
	}
}

RNMountedSurface ReactNativeRootView::_attach_surface(SurfaceId p_surface, std::shared_ptr<const RNComponentViewFactories> p_factories) {
	_detach_surface();
	surface_id = p_surface;
	root_component = std::make_unique<RNRootComponentView>(p_surface, container);
	root_component->set_size_listener([this](const Size2 &p_size) { _content_size_changed(p_size); });
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
// Keys, back presses and gamepad presses go to the focused React view. Godot
// controls inside a GodotScene keep theirs.
void ReactNativeRootView::_begin_input(const Ref<InputEvent> &p_event, ObjectID p_viewport_id) {
	++input_serial;
	if (Ref<InputEventMouseMotion>(p_event).is_valid()) {
		callable_mp(this, &ReactNativeRootView::_route_unclaimed_motion).call_deferred(p_event, input_serial, p_viewport_id);
		return;
	}
	Control *focus_owner = views && get_viewport() ? get_viewport()->gui_get_focus_owner() : nullptr;
	RNComponentView *focused = focus_owner && container->is_ancestor_of(focus_owner) ? views->find_for_control(focus_owner) : nullptr;
	if (!focused) {
		return;
	}
	ReactNativeHost *host = ReactNativeHost::get_singleton();
	if (host && host->handles_back_press() && p_event->is_action_pressed(SNAME("ui_cancel")) && !host->is_default_back_press(p_event)) {
		get_viewport()->set_input_as_handled();
		host->back_pressed(false);
		return;
	}
	const RNInputRouter::Target target = RNInputRouter::target_for(focused, Point2());
	if (Ref<InputEventKey> key = p_event; key.is_valid()) {
		input_router.route_key(key, target);
	} else if (p_event->is_action_released(SNAME("ui_accept"))) {
		input_router.activate(target);
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
	if (contract) {
		return RNError::make(RNErrorCode::VALIDATION, vformat("component '%s' uses a contract", application_key), "scene.attach").to_dictionary();
	}
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
