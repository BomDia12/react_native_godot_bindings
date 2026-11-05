#include "rn_input_event_value.h"

namespace {

Dictionary modifiers_of(const InputEventWithModifiers *p_event) {
	Dictionary modifiers;
	modifiers["alt"] = p_event->is_alt_pressed();
	modifiers["ctrl"] = p_event->is_ctrl_pressed();
	modifiers["shift"] = p_event->is_shift_pressed();
	modifiers["meta"] = p_event->is_meta_pressed();
	return modifiers;
}

void apply_modifiers(InputEventWithModifiers *p_event, const Dictionary &p_value) {
	const Dictionary modifiers = p_value.get("modifiers", Dictionary());
	p_event->set_alt_pressed(modifiers.get("alt", false));
	p_event->set_ctrl_pressed(modifiers.get("ctrl", false));
	p_event->set_shift_pressed(modifiers.get("shift", false));
	p_event->set_meta_pressed(modifiers.get("meta", false));
}

} // namespace

Dictionary rn_input_event_to_value(const Ref<InputEvent> &p_event) {
	Dictionary value;
	if (const Ref<InputEventKey> key = p_event; key.is_valid()) {
		value["type"] = "key";
		value["physicalKeycode"] = int64_t(key->get_physical_keycode());
		value["keycode"] = int64_t(key->get_keycode());
		value["modifiers"] = modifiers_of(key.ptr());
	} else if (const Ref<InputEventMouseButton> button = p_event; button.is_valid()) {
		value["type"] = "mouseButton";
		value["button"] = int64_t(button->get_button_index());
		value["modifiers"] = modifiers_of(button.ptr());
	} else if (const Ref<InputEventJoypadButton> joypad_button = p_event; joypad_button.is_valid()) {
		value["type"] = "joypadButton";
		value["device"] = joypad_button->get_device();
		value["button"] = int64_t(joypad_button->get_button_index());
	} else if (const Ref<InputEventJoypadMotion> motion = p_event; motion.is_valid()) {
		value["type"] = "joypadMotion";
		value["device"] = motion->get_device();
		value["axis"] = int64_t(motion->get_axis());
		value["axisValue"] = motion->get_axis_value();
	} else {
		return value;
	}
	value["label"] = p_event->as_text();
	return value;
}

Ref<InputEvent> rn_input_event_from_value(const Dictionary &p_value, RNError &r_error) {
	const String type = p_value.get("type", String());
	if (type == "key") {
		Ref<InputEventKey> key;
		key.instantiate();
		key->set_physical_keycode(Key(int64_t(p_value.get("physicalKeycode", 0))));
		key->set_keycode(Key(int64_t(p_value.get("keycode", 0))));
		apply_modifiers(key.ptr(), p_value);
		return key;
	}
	if (type == "mouseButton") {
		Ref<InputEventMouseButton> button;
		button.instantiate();
		button->set_button_index(MouseButton(int64_t(p_value.get("button", 0))));
		apply_modifiers(button.ptr(), p_value);
		return button;
	}
	if (type == "joypadButton") {
		Ref<InputEventJoypadButton> button;
		button.instantiate();
		button->set_device(p_value.get("device", -1));
		button->set_button_index(JoyButton(int64_t(p_value.get("button", 0))));
		return button;
	}
	if (type == "joypadMotion") {
		Ref<InputEventJoypadMotion> motion;
		motion.instantiate();
		motion->set_device(p_value.get("device", -1));
		motion->set_axis(JoyAxis(int64_t(p_value.get("axis", 0))));
		motion->set_axis_value(p_value.get("axisValue", 0.0));
		return motion;
	}
	r_error = RNError::make(RNErrorCode::VALIDATION, vformat("'%s' is not an input event type.", type));
	return Ref<InputEvent>();
}
