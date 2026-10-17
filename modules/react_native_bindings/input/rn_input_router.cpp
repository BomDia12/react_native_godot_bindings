#include "rn_input_router.h"

#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "scene/gui/control.h"

#include <react/renderer/components/view/HostPlatformViewEventEmitter.h>

using namespace facebook::react;

namespace {

constexpr int MOUSE_POINTER_ID = 1;
constexpr int MOUSE_TOUCH_ID = 0;

int mouse_button(MouseButton p_button) {
	switch (p_button) {
		case MouseButton::LEFT:
			return 0;
		case MouseButton::MIDDLE:
			return 1;
		case MouseButton::RIGHT:
			return 2;
		default:
			return -1;
	}
}

int mouse_button_mask(MouseButton p_button) {
	switch (p_button) {
		case MouseButton::LEFT:
			return 1;
		case MouseButton::RIGHT:
			return 2;
		case MouseButton::MIDDLE:
			return 4;
		default:
			return 0;
	}
}

struct KeyNames {
	Key key;
	const char *name;
	const char *code;
};

const KeyNames *special_key_names(Key p_key) {
	static constexpr KeyNames keys[] = {
		{ Key::ENTER, "Enter", "Enter" },
		{ Key::KP_ENTER, "Enter", "Enter" },
		{ Key::SPACE, " ", "Space" },
		{ Key::ESCAPE, "Escape", "Escape" },
		{ Key::TAB, "Tab", "Tab" },
		{ Key::BACKSPACE, "Backspace", "Backspace" },
		{ Key::KEY_DELETE, "Delete", "Delete" },
		{ Key::LEFT, "ArrowLeft", "ArrowLeft" },
		{ Key::RIGHT, "ArrowRight", "ArrowRight" },
		{ Key::UP, "ArrowUp", "ArrowUp" },
		{ Key::DOWN, "ArrowDown", "ArrowDown" },
		{ Key::HOME, "Home", "Home" },
		{ Key::END, "End", "End" },
		{ Key::PAGEUP, "PageUp", "PageUp" },
		{ Key::PAGEDOWN, "PageDown", "PageDown" },
	};
	for (const KeyNames &entry : keys) {
		if (entry.key == p_key) {
			return &entry;
		}
	}
	return nullptr;
}

std::string key_name(const Ref<InputEventKey> &p_key) {
	if (const KeyNames *names = special_key_names(p_key->get_keycode())) {
		return names->name;
	}
	if (p_key->get_unicode() != 0) {
		return String::chr(p_key->get_unicode()).utf8().get_data();
	}
	const String value = keycode_get_string(p_key->get_keycode());
	return value.is_empty() ? "Unidentified" : value.utf8().get_data();
}

std::string code_name(const Ref<InputEventKey> &p_key) {
	const Key physical = p_key->get_physical_keycode() == Key::NONE ? p_key->get_keycode() : p_key->get_physical_keycode();
	const int code = int(physical);
	if (code >= int(Key::A) && code <= int(Key::Z)) {
		return std::string("Key") + char(code);
	}
	if (code >= int(Key::KEY_0) && code <= int(Key::KEY_9)) {
		return std::string("Digit") + char(code);
	}
	if (const KeyNames *names = special_key_names(physical)) {
		return names->code;
	}
	const String value = keycode_get_string(physical);
	return value.is_empty() ? "Unidentified" : value.utf8().get_data();
}

std::shared_ptr<const HostPlatformViewEventEmitter> view_emitter(const RNInputRouter::Target &p_target) {
	return std::dynamic_pointer_cast<const HostPlatformViewEventEmitter>(p_target.emitter);
}

} // namespace

RNInputRouter::Target RNInputRouter::target_for(const RNComponentView *p_view, const Point2 &p_local) {
	if (!p_view) {
		return Target();
	}
	const RNTouchTarget touch_target = p_view->touch_target_at(p_local);
	Control *focus = p_view->get_focus_control();
	return { touch_target.tag, touch_target.emitter, p_view->get_control()->get_instance_id(), focus ? focus->get_instance_id() : ObjectID(), p_view->owns_activation() };
}

Point RNInputRouter::point(const Point2 &p_point) {
	return { .x = p_point.x, .y = p_point.y };
}

Point2 RNInputRouter::offset_in(const Target &p_target, const Position &p_position) {
	Control *control = Object::cast_to<Control>(ObjectDB::get_instance(p_target.control));
	return control ? control->get_global_transform_with_canvas().affine_inverse().xform(p_position.viewport) : p_position.page;
}

Touch RNInputRouter::touch(const Target &p_target, const Position &p_position, int p_identifier, float p_force) {
	Touch result;
	result.pagePoint = point(p_position.page);
	result.offsetPoint = point(offset_in(p_target, p_position));
	result.screenPoint = point(p_position.screen);
	result.identifier = p_identifier;
	result.target = p_target.tag;
	result.force = p_force;
	result.timestamp = Float(OS::get_singleton()->get_ticks_msec());
	result.timeStamp = HighResTimeStamp::now();
	return result;
}

PointerEvent RNInputRouter::pointer(const Target &p_target, const Position &p_position, int p_pointer_id, const char *p_type, int p_button, int p_buttons, float p_pressure, bool p_primary, const InputEventWithModifiers *p_modifiers) {
	PointerEvent result{};
	result.pointerId = p_pointer_id;
	result.pressure = p_pressure;
	result.pointerType = p_type;
	result.clientPoint = point(p_position.page);
	result.screenPoint = point(p_position.screen);
	result.offsetPoint = point(offset_in(p_target, p_position));
	result.width = 1;
	result.height = 1;
	result.buttons = p_buttons;
	result.button = p_button;
	result.isPrimary = p_primary;
	result.ctrlKey = p_modifiers && p_modifiers->is_ctrl_pressed();
	result.shiftKey = p_modifiers && p_modifiers->is_shift_pressed();
	result.altKey = p_modifiers && p_modifiers->is_alt_pressed();
	result.metaKey = p_modifiers && p_modifiers->is_meta_pressed();
	result.timeStamp = HighResTimeStamp::now();
	return result;
}

TouchEvent RNInputRouter::touch_event(const Touch &p_changed, bool p_ended) const {
	TouchEvent event;
	event.changedTouches.insert(p_changed);
	for (const KeyValue<int, Contact> &entry : touches) {
		const Touch current = touch(entry.value.target, entry.value.position, entry.key, entry.value.pressure);
		event.touches.insert(current);
		if (current.target == p_changed.target) {
			event.targetTouches.insert(current);
		}
	}
	if (mouse_active.is_valid()) {
		const Touch current = touch(mouse_active, mouse_position, MOUSE_TOUCH_ID, 0.5f);
		event.touches.insert(current);
		event.targetTouches.insert(current);
	}
	if (p_ended) {
		event.touches.erase(p_changed);
		event.targetTouches.erase(p_changed);
	} else {
		event.touches.insert(p_changed);
		event.targetTouches.insert(p_changed);
	}
	return event;
}

void RNInputRouter::_hover(const Target &p_target, const Position &p_position, const InputEventWithModifiers *p_modifiers) {
	if (p_target.tag == hover.tag && p_target.emitter == hover.emitter) {
		return;
	}
	if (hover.is_valid()) {
		const PointerEvent event = pointer(hover, p_position, MOUSE_POINTER_ID, "mouse", -1, mouse_buttons, 0, true, p_modifiers);
		hover.emitter->onPointerOut(event);
		hover.emitter->onPointerLeave(event);
	}
	hover = p_target;
	if (hover.is_valid()) {
		const PointerEvent event = pointer(hover, p_position, MOUSE_POINTER_ID, "mouse", -1, mouse_buttons, mouse_buttons ? 0.5f : 0, true, p_modifiers);
		hover.emitter->onPointerOver(event);
		hover.emitter->onPointerEnter(event);
	}
}

void RNInputRouter::_mouse_motion(const Ref<InputEventMouseMotion> &p_motion, const Target &p_target, const Position &p_position) {
	mouse_position = p_position;
	_hover(p_target, p_position, p_motion.ptr());
	const Target &target = mouse_active.is_valid() ? mouse_active : p_target;
	if (!target.is_valid()) {
		return;
	}
	target.emitter->onPointerMove(pointer(target, p_position, MOUSE_POINTER_ID, "mouse", -1, mouse_buttons, mouse_buttons ? 0.5f : 0, true, p_motion.ptr()));
	if (mouse_active.is_valid() && !mouse_active.owns_activation) {
		mouse_active.emitter->onTouchMove(touch_event(touch(mouse_active, p_position, MOUSE_TOUCH_ID, 0.5f), false));
	}
}

Control *RNInputRouter::_mouse_button(const Ref<InputEventMouseButton> &p_button, const Target &p_target, const Position &p_position) {
	const int button = mouse_button(p_button->get_button_index());
	const int mask = mouse_button_mask(p_button->get_button_index());
	if (button < 0) {
		return nullptr;
	}
	mouse_position = p_position;
	if (p_button->is_pressed()) {
		mouse_buttons |= mask;
	} else {
		mouse_buttons &= ~mask;
	}
	if (mouse_buttons == 0) {
		mouse_pressed = Target();
	}
	const bool primary_release = button == 0 && !p_button->is_pressed();
	const Target target = primary_release && mouse_active.is_valid() ? mouse_active : p_target;
	if (!target.is_valid()) {
		if (primary_release) {
			mouse_active = Target();
		}
		return nullptr;
	}

	if (p_button->is_pressed()) {
		mouse_pressed = target;
		const PointerEvent event = pointer(target, p_position, MOUSE_POINTER_ID, "mouse", button, mouse_buttons, 0.5f, true, p_button.ptr());
		target.emitter->onPointerDown(event);
		if (button == 1 || button == 2) {
			if (auto emitter = view_emitter(target)) {
				if (button == 1) {
					emitter->onMiddleClick(event);
				} else {
					emitter->onRightClick(event);
				}
			}
			return nullptr;
		}
		mouse_active = target;
		if (target.owns_activation) {
			return nullptr;
		}
		target.emitter->onTouchStart(touch_event(touch(target, p_position, MOUSE_TOUCH_ID, 0.5f), false));
		return Object::cast_to<Control>(ObjectDB::get_instance(target.focus));
	}

	const PointerEvent event = pointer(target, p_position, MOUSE_POINTER_ID, "mouse", button, mouse_buttons, 0, true, p_button.ptr());
	if (button != 0) {
		target.emitter->onPointerUp(event);
		return nullptr;
	}
	mouse_active = Target();
	if (p_button->is_canceled()) {
		if (!target.owns_activation) {
			target.emitter->onTouchCancel(touch_event(touch(target, p_position, MOUSE_TOUCH_ID, 0), true));
		}
		target.emitter->onPointerCancel(event);
		return nullptr;
	}
	if (!target.owns_activation) {
		target.emitter->onTouchEnd(touch_event(touch(target, p_position, MOUSE_TOUCH_ID, 0), true));
	}
	target.emitter->onPointerUp(event);
	if (!target.owns_activation && p_target.tag == target.tag) {
		PointerEvent click = event;
		click.isPrimary = false;
		target.emitter->onClick(click);
	}
	return nullptr;
}

Control *RNInputRouter::_screen_touch(const Ref<InputEventScreenTouch> &p_touch, const Target &p_target, const Position &p_position) {
	const int index = p_touch->get_index();
	if (p_touch->is_pressed()) {
		if (!p_target.is_valid()) {
			return nullptr;
		}
		if (touches.is_empty()) {
			primary_touch = index;
		}
		touches[index] = { p_target, p_position, 0.5f };
		p_target.emitter->onPointerDown(pointer(p_target, p_position, index, "touch", 0, 1, 0.5f, index == primary_touch, nullptr));
		if (p_target.owns_activation) {
			return nullptr;
		}
		p_target.emitter->onTouchStart(touch_event(touch(p_target, p_position, index, 0.5f), false));
		return Object::cast_to<Control>(ObjectDB::get_instance(p_target.focus));
	}

	const Contact *contact = touches.getptr(index);
	if (!contact) {
		return nullptr;
	}
	const Target target = contact->target;
	const bool primary = index == primary_touch;
	touches.erase(index);
	// A pointer stays primary for its whole lifetime, so nothing becomes primary
	// again until every contact is released.
	if (primary) {
		primary_touch = -1;
	}
	const PointerEvent event = pointer(target, p_position, index, "touch", 0, 0, 0, primary, nullptr);
	const Touch ended = touch(target, p_position, index, 0);
	if (p_touch->is_canceled()) {
		if (!target.owns_activation) {
			target.emitter->onTouchCancel(touch_event(ended, true));
		}
		target.emitter->onPointerCancel(event);
	} else {
		if (!target.owns_activation) {
			target.emitter->onTouchEnd(touch_event(ended, true));
		}
		target.emitter->onPointerUp(event);
	}
	return nullptr;
}

void RNInputRouter::_screen_drag(const Ref<InputEventScreenDrag> &p_drag, const Position &p_position) {
	Contact *contact = touches.getptr(p_drag->get_index());
	if (!contact) {
		return;
	}
	contact->position = p_position;
	contact->pressure = p_drag->get_pressure() > 0 ? p_drag->get_pressure() : 0.5f;
	const Target &target = contact->target;
	const int index = p_drag->get_index();
	target.emitter->onPointerMove(pointer(target, p_position, index, "touch", -1, 1, contact->pressure, index == primary_touch, nullptr));
	if (!target.owns_activation) {
		target.emitter->onTouchMove(touch_event(touch(target, p_position, index, contact->pressure), false));
	}
}

Control *RNInputRouter::route_pointer(const Ref<InputEvent> &p_event, const Target &p_target, const Position &p_position) {
	if (Ref<InputEventMouseMotion> motion = p_event; motion.is_valid()) {
		_mouse_motion(motion, p_target, p_position);
	} else if (Ref<InputEventMouseButton> button = p_event; button.is_valid()) {
		return _mouse_button(button, p_target, p_position);
	} else if (Ref<InputEventScreenTouch> screen_touch = p_event; screen_touch.is_valid()) {
		return _screen_touch(screen_touch, p_target, p_position);
	} else if (Ref<InputEventScreenDrag> drag = p_event; drag.is_valid()) {
		_screen_drag(drag, p_position);
	}
	return nullptr;
}

void RNInputRouter::route_key(const Ref<InputEventKey> &p_key, const Target &p_target) {
	auto emitter = view_emitter(p_target);
	if (p_key.is_null() || !emitter) {
		return;
	}
	const GodotKeyEvent event{
		.key = key_name(p_key),
		.code = code_name(p_key),
		.altKey = p_key->is_alt_pressed(),
		.ctrlKey = p_key->is_ctrl_pressed(),
		.metaKey = p_key->is_meta_pressed(),
		.shiftKey = p_key->is_shift_pressed(),
		.repeat = p_key->is_echo(),
	};
	if (p_key->is_pressed()) {
		emitter->onKeyDown(event);
		return;
	}
	emitter->onKeyUp(event);
	const Key key = p_key->get_keycode();
	if (!p_target.owns_activation && !p_key->is_echo() && (key == Key::ENTER || key == Key::KP_ENTER || key == Key::SPACE)) {
		emitter->onKeyboardClick();
	}
}

void RNInputRouter::_cancel(const Target &p_target, const Position &p_position, int p_touch, int p_pointer, const char *p_type, bool p_primary) {
	if (!p_target.owns_activation) {
		p_target.emitter->onTouchCancel(touch_event(touch(p_target, p_position, p_touch, 0), true));
	}
	p_target.emitter->onPointerCancel(pointer(p_target, p_position, p_pointer, p_type, 0, 0, 0, p_primary, nullptr));
}

void RNInputRouter::view_destroyed(Tag p_tag) {
	if (hover.tag == p_tag) {
		hover = Target();
	}
	if (mouse_pressed.tag == p_tag) {
		mouse_pressed = Target();
	}
	if (mouse_active.tag == p_tag) {
		const Target target = mouse_active;
		mouse_active = Target();
		mouse_buttons = 0;
		_cancel(target, mouse_position, MOUSE_TOUCH_ID, MOUSE_POINTER_ID, "mouse", true);
	}
	::Vector<int> ended;
	for (const KeyValue<int, Contact> &entry : touches) {
		if (entry.value.target.tag == p_tag) {
			ended.push_back(entry.key);
		}
	}
	for (int index : ended) {
		const Contact contact = touches[index];
		touches.erase(index);
		_cancel(contact.target, contact.position, index, index, "touch", index == primary_touch);
		if (index == primary_touch) {
			primary_touch = -1;
		}
	}
}

// Something outside React took the mouse mid-press, such as a popup menu the
// press opened, so the release will never come back here.
void RNInputRouter::cancel_mouse() {
	const Target active = mouse_active;
	const Target pressed = mouse_pressed;
	mouse_active = Target();
	mouse_pressed = Target();
	mouse_buttons = 0;
	if (active.is_valid()) {
		_cancel(active, mouse_position, MOUSE_TOUCH_ID, MOUSE_POINTER_ID, "mouse", true);
	} else if (pressed.is_valid()) {
		pressed.emitter->onPointerCancel(pointer(pressed, mouse_position, MOUSE_POINTER_ID, "mouse", 0, 0, 0, true, nullptr));
	}
}

void RNInputRouter::cancel_all() {
	_hover(Target(), mouse_position, nullptr);
	cancel_mouse();
	const HashMap<int, Contact> active(touches);
	touches.clear();
	for (const KeyValue<int, Contact> &entry : active) {
		_cancel(entry.value.target, entry.value.position, entry.key, entry.key, "touch", entry.key == primary_touch);
	}
	primary_touch = -1;
}
