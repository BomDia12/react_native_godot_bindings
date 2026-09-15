#include "rn_input_router.h"

#include "../fabric/fabric_ui_manager.h"

#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "scene/gui/control.h"

namespace {

String pointer_events_of(const Ref<RNShadowNode> &p_node) {
	return String(p_node->props.get("pointerEvents", "auto")).to_lower();
}

bool node_is_visible(const Ref<RNShadowNode> &p_node, const RNRegistry *p_registry) {
	if (String(p_node->props.get("display", "flex")) == "none") {
		return false;
	}
	if (!p_registry) {
		return true;
	}
	Control *control = Object::cast_to<Control>(p_registry->get_node(p_node->tag));
	return !control || control->is_visible_in_tree();
}

Rect2 clipped(const Rect2 &p_left, const Rect2 &p_right) {
	return p_left.intersection(p_right);
}

Rect2 hit_rect(const Ref<RNShadowNode> &p_node, const Point2 &p_origin) {
	Rect2 result(p_origin, p_node->layout.size);
	const Variant hit_slop_value = p_node->props.get("hitSlop", Variant());
	if (hit_slop_value.get_type() == Variant::INT || hit_slop_value.get_type() == Variant::FLOAT) {
		const float amount = float(hit_slop_value);
		result.position -= Point2(amount, amount);
		result.size += Size2(amount * 2.0f, amount * 2.0f);
	} else if (hit_slop_value.get_type() == Variant::DICTIONARY) {
		const Dictionary hit_slop = hit_slop_value;
		const float left = float(hit_slop.get("left", 0.0));
		const float top = float(hit_slop.get("top", 0.0));
		const float right = float(hit_slop.get("right", 0.0));
		const float bottom = float(hit_slop.get("bottom", 0.0));
		result.position -= Point2(left, top);
		result.size += Size2(left + right, top + bottom);
	}
	return result;
}

bool clips_children(const Ref<RNShadowNode> &p_node) {
	return String(p_node->props.get("overflow", "visible")) == "hidden";
}

uint64_t timestamp_now() {
	return OS::get_singleton()->get_ticks_msec();
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

} // namespace

RNNativeEvent RNInputRouter::event(int p_tag, const String &p_name, int p_priority, uint64_t p_generation, const Dictionary &p_payload) {
	RNNativeEvent result;
	result.tag = p_tag;
	result.name = p_name;
	result.priority = p_priority;
	result.generation = p_generation;
	result.payload = p_payload;
	return result;
}

int RNInputRouter::hit_test_node(const Ref<RNShadowNode> &p_node, const Point2 &p_point, const Point2 &p_parent_origin, const Rect2 &p_clip, const RNRegistry *p_registry) {
	if (p_node.is_null() || !node_is_visible(p_node, p_registry) || !p_clip.has_point(p_point)) {
		return 0;
	}

	const Point2 origin = p_parent_origin + p_node->layout.position;
	const Rect2 bounds(origin, p_node->layout.size);
	const String pointer_events = pointer_events_of(p_node);
	if (pointer_events == "none") {
		return 0;
	}

	Rect2 child_clip = p_clip;
	if (clips_children(p_node)) {
		child_clip = clipped(child_clip, bounds);
	}

	if (pointer_events != "box-only") {
		for (int i = p_node->children.size() - 1; i >= 0; --i) {
			const int child_tag = hit_test_node(p_node->children[i], p_point, origin, child_clip, p_registry);
			if (child_tag != 0) {
				return child_tag;
			}
		}
	}

	if (pointer_events != "box-none" && p_node->view_name == "RCTView" && hit_rect(p_node, origin).has_point(p_point)) {
		return p_node->tag;
	}
	return 0;
}

int RNInputRouter::hit_test(const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, const Size2 &p_root_size, const Point2 &p_point) const {
	if (p_tree.is_null()) {
		return 0;
	}
	const Rect2 root_bounds(Point2(), p_root_size);
	for (int i = p_tree->children.size() - 1; i >= 0; --i) {
		const int tag = hit_test_node(p_tree->children[i], p_point, Point2(), root_bounds, &p_registry);
		if (tag != 0) {
			return tag;
		}
	}
	return 0;
}

int RNInputRouter::hit_test(const Ref<RNShadowNode> &p_tree, const Size2 &p_root_size, const Point2 &p_point) {
	if (p_tree.is_null()) {
		return 0;
	}
	const Rect2 root_bounds(Point2(), p_root_size);
	for (int i = p_tree->children.size() - 1; i >= 0; --i) {
		const int tag = hit_test_node(p_tree->children[i], p_point, Point2(), root_bounds, nullptr);
		if (tag != 0) {
			return tag;
		}
	}
	return 0;
}

bool RNInputRouter::find_origin(const Ref<RNShadowNode> &p_node, int p_tag, const Point2 &p_parent_origin, Point2 &r_origin) {
	if (p_node.is_null()) {
		return false;
	}
	const Point2 origin = p_parent_origin + p_node->layout.position;
	if (p_node->tag == p_tag) {
		r_origin = origin;
		return true;
	}
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (find_origin(child, p_tag, origin, r_origin)) {
			return true;
		}
	}
	return false;
}

Point2 RNInputRouter::target_origin(const Ref<RNShadowNode> &p_tree, int p_tag) const {
	Point2 origin;
	find_origin(p_tree, p_tag, Point2(), origin);
	return origin;
}

Dictionary RNInputRouter::pointer_payload(const PointerSample &p_sample) {
	Dictionary payload;
	payload["target"] = p_sample.tag;
	payload["timestamp"] = int64_t(p_sample.timestamp);
	payload["detail"] = 0;
	payload["screenX"] = p_sample.screen_position.x;
	payload["screenY"] = p_sample.screen_position.y;
	payload["clientX"] = p_sample.root_position.x;
	payload["clientY"] = p_sample.root_position.y;
	payload["x"] = p_sample.root_position.x;
	payload["y"] = p_sample.root_position.y;
	payload["pageX"] = p_sample.root_position.x;
	payload["pageY"] = p_sample.root_position.y;
	payload["offsetX"] = p_sample.root_position.x - p_sample.target_origin.x;
	payload["offsetY"] = p_sample.root_position.y - p_sample.target_origin.y;
	payload["altKey"] = p_sample.modifiers && p_sample.modifiers->is_alt_pressed();
	payload["ctrlKey"] = p_sample.modifiers && p_sample.modifiers->is_ctrl_pressed();
	payload["metaKey"] = p_sample.modifiers && p_sample.modifiers->is_meta_pressed();
	payload["shiftKey"] = p_sample.modifiers && p_sample.modifiers->is_shift_pressed();
	payload["button"] = p_sample.button;
	payload["buttons"] = p_sample.buttons;
	payload["relatedTarget"] = Variant();
	payload["pointerId"] = p_sample.pointer_id;
	payload["width"] = p_sample.width;
	payload["height"] = p_sample.height;
	payload["pressure"] = p_sample.pressure;
	payload["tangentialPressure"] = 0.0;
	payload["tiltX"] = 0.0;
	payload["tiltY"] = 0.0;
	payload["twist"] = 0.0;
	payload["pointerType"] = p_sample.pointer_type;
	payload["isPrimary"] = p_sample.primary;
	return payload;
}

Dictionary RNInputRouter::touch_value(const PointerSample &p_sample, int p_root_tag) {
	Dictionary touch;
	touch["identifier"] = p_sample.pointer_id;
	touch["locationX"] = p_sample.root_position.x - p_sample.target_origin.x;
	touch["locationY"] = p_sample.root_position.y - p_sample.target_origin.y;
	touch["pageX"] = p_sample.root_position.x;
	touch["pageY"] = p_sample.root_position.y;
	touch["screenX"] = p_sample.screen_position.x;
	touch["screenY"] = p_sample.screen_position.y;
	touch["target"] = p_sample.tag;
	touch["targetSurface"] = p_root_tag;
	touch["timestamp"] = int64_t(p_sample.timestamp);
	touch["force"] = p_sample.pressure;
	return touch;
}

Dictionary RNInputRouter::touch_payload(const Dictionary &p_touch, const Array &p_touches) {
	Dictionary payload = p_touch.duplicate(true);
	Array changed;
	changed.push_back(p_touch.duplicate(true));
	payload["changedTouches"] = changed;
	payload["touches"] = p_touches;
	return payload;
}

int RNInputRouter::mouse_button(MouseButton p_button) {
	if (p_button == MouseButton::LEFT) {
		return 0;
	}
	if (p_button == MouseButton::MIDDLE) {
		return 1;
	}
	if (p_button == MouseButton::RIGHT) {
		return 2;
	}
	return -1;
}

int RNInputRouter::mouse_button_mask(MouseButton p_button) {
	if (p_button == MouseButton::LEFT) {
		return 1;
	}
	if (p_button == MouseButton::RIGHT) {
		return 2;
	}
	if (p_button == MouseButton::MIDDLE) {
		return 4;
	}
	return 0;
}

void RNInputRouter::append_mouse_hover(RouteResult &r_result, const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, const Size2 &p_root_size, const Point2 &p_root_position, const Point2 &p_screen_position, const InputEventWithModifiers *p_modifiers, uint64_t p_generation, uint64_t p_timestamp) {
	const int next_hover = hit_test(p_tree, p_registry, p_root_size, p_root_position);
	if (next_hover == hover_tag) {
		return;
	}

	if (hover_tag != 0) {
		const Dictionary payload = pointer_payload({ hover_tag, p_root_position, p_screen_position, target_origin(p_tree, hover_tag), -1, mouse_buttons, "mouse", 1, 0.0f, 1.0f, 1.0f, true, p_modifiers, p_timestamp });
		r_result.events.push_back(event(hover_tag, "topPointerOut", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
		r_result.events.push_back(event(hover_tag, "topPointerLeave", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
	}
	if (next_hover != 0) {
		const Dictionary payload = pointer_payload({ next_hover, p_root_position, p_screen_position, target_origin(p_tree, next_hover), -1, mouse_buttons, "mouse", 1, mouse_buttons ? 0.5f : 0.0f, 1.0f, 1.0f, true, p_modifiers, p_timestamp });
		r_result.events.push_back(event(next_hover, "topPointerOver", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
		r_result.events.push_back(event(next_hover, "topPointerEnter", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
	}
	hover_tag = next_hover;
}

Array RNInputRouter::current_touches(const Ref<RNShadowNode> &p_tree, int p_root_tag, uint64_t p_timestamp) const {
	Array touches;
	for (const KeyValue<int, TouchContact> &entry : touch_contacts) {
		const TouchContact &contact = entry.value;
		PointerSample sample{ contact.tag, contact.root_position, contact.screen_position, target_origin(p_tree, contact.tag), -1, 0, "touch", entry.key, contact.pressure, contact.size, contact.size, entry.key == primary_touch_id, nullptr, p_timestamp };
		touches.push_back(touch_value(sample, p_root_tag));
	}
	return touches;
}

RNInputRouter::RouteResult RNInputRouter::route_pointer(const Ref<InputEvent> &p_event, const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, const Size2 &p_root_size, int p_root_tag, uint64_t p_generation, const Point2 &p_root_position, const Point2 &p_screen_position) {
	RouteResult result;
	const uint64_t timestamp = timestamp_now();

	if (Ref<InputEventMouseMotion> motion = p_event; motion.is_valid()) {
		mouse_root_position = p_root_position;
		mouse_screen_position = p_screen_position;
		append_mouse_hover(result, p_tree, p_registry, p_root_size, p_root_position, p_screen_position, motion.ptr(), p_generation, timestamp);
		const int target = mouse_active_tag != 0 ? mouse_active_tag : hit_test(p_tree, p_registry, p_root_size, p_root_position);
		if (target != 0) {
			PointerSample sample{ target, p_root_position, p_screen_position, target_origin(p_tree, target), -1, mouse_buttons, "mouse", 1, mouse_buttons ? 0.5f : 0.0f, 1.0f, 1.0f, true, motion.ptr(), timestamp };
			const Dictionary payload = pointer_payload(sample);
			result.events.push_back(event(target, "topPointerMove", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
			if (mouse_active_tag != 0) {
				sample.pointer_id = 0;
				sample.pressure = 0.5f;
				const Dictionary touch = touch_value(sample, p_root_tag);
				Array touches;
				touches.push_back(touch.duplicate(true));
				const Dictionary touch_event_payload = touch_payload(touch, touches);
				result.events.push_back(event(target, "topTouchMove", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, touch_event_payload));
			}
			result.accepted = true;
		}
		return result;
	}

	if (Ref<InputEventMouseButton> button_event = p_event; button_event.is_valid()) {
		const int button = mouse_button(button_event->get_button_index());
		const int mask = mouse_button_mask(button_event->get_button_index());
		if (button < 0 || mask == 0) {
			return result;
		}
		mouse_root_position = p_root_position;
		mouse_screen_position = p_screen_position;
		if (button_event->is_pressed()) {
			mouse_buttons |= mask;
		} else {
			mouse_buttons &= ~mask;
		}

		const int hit = hit_test(p_tree, p_registry, p_root_size, p_root_position);
		const int target = button == 0 && !button_event->is_pressed() && mouse_active_tag != 0 ? mouse_active_tag : hit;
		if (target == 0) {
			if (button == 0 && !button_event->is_pressed()) {
				mouse_active_tag = 0;
			}
			return result;
		}

		const Point2 origin = target_origin(p_tree, target);
		PointerSample sample{ target, p_root_position, p_screen_position, origin, button, mouse_buttons, "mouse", 1, button_event->is_pressed() ? 0.5f : 0.0f, 1.0f, 1.0f, true, button_event.ptr(), timestamp };
		if (button_event->is_pressed()) {
			const Dictionary payload = pointer_payload(sample);
			result.events.push_back(event(target, "topPointerDown", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, payload));
			if (button == 0) {
				mouse_active_tag = target;
				result.focus_tag = target;
				sample.pointer_id = 0;
				const Dictionary touch = touch_value(sample, p_root_tag);
				Array values;
				values.push_back(touch.duplicate(true));
				result.events.push_back(event(target, "topTouchStart", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, values.duplicate(true))));
			}
		} else {
			if (button == 0 && button_event->is_canceled()) {
				sample.pointer_id = 0;
				const Dictionary touch = touch_value(sample, p_root_tag);
				result.events.push_back(event(target, "topTouchCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, Array())));
				sample.pointer_id = 1;
				result.events.push_back(event(target, "topPointerCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer_payload(sample)));
			} else {
				if (button == 0) {
					sample.pointer_id = 0;
					const Dictionary touch = touch_value(sample, p_root_tag);
					result.events.push_back(event(target, "topTouchEnd", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, Array())));
				}
				sample.pointer_id = 1;
				const Dictionary payload = pointer_payload(sample);
				result.events.push_back(event(target, "topPointerUp", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, payload));
				if (button == 0 && hit == mouse_active_tag) {
					Dictionary click = payload.duplicate(true);
					click["isPrimary"] = false;
					result.events.push_back(event(target, "topClick", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, click));
				}
			}
			if (button == 0) {
				mouse_active_tag = 0;
			}
		}
		result.accepted = true;
		return result;
	}

	if (Ref<InputEventScreenTouch> screen_touch = p_event; screen_touch.is_valid()) {
		const int index = screen_touch->get_index();
		if (screen_touch->is_pressed()) {
			const int target = hit_test(p_tree, p_registry, p_root_size, p_root_position);
			if (target == 0) {
				return result;
			}
			TouchContact contact;
			contact.tag = target;
			contact.root_position = p_root_position;
			contact.screen_position = p_screen_position;
			const bool is_first_contact = touch_contacts.is_empty();
			touch_contacts[index] = contact;
			if (is_first_contact) {
				primary_touch_id = index;
			}
			PointerSample sample{ target, p_root_position, p_screen_position, target_origin(p_tree, target), 0, 1, "touch", index, 0.5f, 1.0f, 1.0f, index == primary_touch_id, nullptr, timestamp };
			const Dictionary pointer = pointer_payload(sample);
			result.events.push_back(event(target, "topPointerDown", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer));
			const Dictionary touch = touch_value(sample, p_root_tag);
			result.events.push_back(event(target, "topTouchStart", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, current_touches(p_tree, p_root_tag, timestamp))));
			result.focus_tag = target;
			result.accepted = true;
			return result;
		}

		TouchContact *contact = touch_contacts.getptr(index);
		if (!contact) {
			return result;
		}
		contact->root_position = p_root_position;
		contact->screen_position = p_screen_position;
		const TouchContact ended = *contact;
		const bool was_primary = index == primary_touch_id;
		touch_contacts.erase(index);
		// A pointer is primary for its whole lifetime (W3C): once the primary contact
		// lifts, nothing is primary again until every contact has been released.
		if (was_primary) {
			primary_touch_id = -1;
		}
		PointerSample sample{ ended.tag, p_root_position, p_screen_position, target_origin(p_tree, ended.tag), 0, 0, "touch", index, 0.0f, 1.0f, 1.0f, was_primary, nullptr, timestamp };
		const Dictionary touch = touch_value(sample, p_root_tag);
		const Dictionary payload = touch_payload(touch, current_touches(p_tree, p_root_tag, timestamp));
		const bool canceled = screen_touch->is_canceled();
		result.events.push_back(event(ended.tag, canceled ? "topTouchCancel" : "topTouchEnd", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, payload));
		result.events.push_back(event(ended.tag, canceled ? "topPointerCancel" : "topPointerUp", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer_payload(sample)));
		result.accepted = true;
		return result;
	}

	if (Ref<InputEventScreenDrag> drag = p_event; drag.is_valid()) {
		TouchContact *contact = touch_contacts.getptr(drag->get_index());
		if (!contact) {
			return result;
		}
		contact->root_position = p_root_position;
		contact->screen_position = p_screen_position;
		contact->pressure = drag->get_pressure() > 0.0f ? drag->get_pressure() : 0.5f;
		const int target = contact->tag;
		PointerSample sample{ target, p_root_position, p_screen_position, target_origin(p_tree, target), -1, 1, "touch", drag->get_index(), contact->pressure, 1.0f, 1.0f, drag->get_index() == primary_touch_id, nullptr, timestamp };
		result.events.push_back(event(target, "topPointerMove", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, pointer_payload(sample)));
		const Dictionary touch = touch_value(sample, p_root_tag);
		result.events.push_back(event(target, "topTouchMove", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, touch_payload(touch, current_touches(p_tree, p_root_tag, timestamp))));
		result.accepted = true;
	}
	return result;
}

String RNInputRouter::key_name(const Ref<InputEventKey> &p_key) {
	if (const KeyNames *names = special_key_names(p_key->get_keycode())) {
		return names->name;
	}
	if (p_key->get_unicode() != 0) {
		return String::chr(p_key->get_unicode());
	}
	const String value = keycode_get_string(p_key->get_keycode());
	return value.is_empty() ? "Unidentified" : value;
}

String RNInputRouter::code_name(const Ref<InputEventKey> &p_key) {
	const Key physical = p_key->get_physical_keycode() == Key::NONE ? p_key->get_keycode() : p_key->get_physical_keycode();
	const int code = int(physical);
	if (code >= int(Key::A) && code <= int(Key::Z)) {
		return "Key" + String::chr(char32_t(code));
	}
	if (code >= int(Key::KEY_0) && code <= int(Key::KEY_9)) {
		return "Digit" + String::chr(char32_t(code));
	}
	if (const KeyNames *names = special_key_names(physical)) {
		return names->code;
	}
	const String value = keycode_get_string(physical);
	return value.is_empty() ? "Unidentified" : value;
}

RNInputRouter::RouteResult RNInputRouter::route_key(const Ref<InputEventKey> &p_key, int p_target_tag, uint64_t p_generation) {
	RouteResult result;
	if (p_key.is_null() || p_target_tag == 0) {
		return result;
	}
	Dictionary payload;
	payload["target"] = p_target_tag;
	payload["key"] = key_name(p_key);
	payload["code"] = code_name(p_key);
	payload["altKey"] = p_key->is_alt_pressed();
	payload["ctrlKey"] = p_key->is_ctrl_pressed();
	payload["metaKey"] = p_key->is_meta_pressed();
	payload["shiftKey"] = p_key->is_shift_pressed();
	payload["repeat"] = p_key->is_echo();
	payload["isComposing"] = false;
	result.events.push_back(event(p_target_tag, p_key->is_pressed() ? "topKeyDown" : "topKeyUp", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, payload));
	if (!p_key->is_pressed() && !p_key->is_echo() && (p_key->get_keycode() == Key::ENTER || p_key->get_keycode() == Key::KP_ENTER || p_key->get_keycode() == Key::SPACE)) {
		Dictionary click;
		click["target"] = p_target_tag;
		click["detail"] = 0;
		click["timestamp"] = int64_t(timestamp_now());
		result.events.push_back(event(p_target_tag, "topClick", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, click));
	}
	result.accepted = true;
	return result;
}

Vector<RNNativeEvent> RNInputRouter::cancel_all(const Ref<RNShadowNode> &p_tree, int p_root_tag, uint64_t p_generation) {
	Vector<RNNativeEvent> result;
	const uint64_t timestamp = timestamp_now();
	if (hover_tag != 0) {
		const Dictionary payload = pointer_payload({ hover_tag, mouse_root_position, mouse_screen_position, target_origin(p_tree, hover_tag), -1, mouse_buttons, "mouse", 1, mouse_buttons ? 0.5f : 0.0f, 1.0f, 1.0f, true, nullptr, timestamp });
		result.push_back(event(hover_tag, "topPointerOut", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
		result.push_back(event(hover_tag, "topPointerLeave", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
	}
	if (mouse_active_tag != 0) {
		const Point2 origin = target_origin(p_tree, mouse_active_tag);
		PointerSample sample{ mouse_active_tag, mouse_root_position, mouse_screen_position, origin, 0, 0, "mouse", 0, 0.0f, 1.0f, 1.0f, true, nullptr, timestamp };
		const Dictionary touch = touch_value(sample, p_root_tag);
		result.push_back(event(mouse_active_tag, "topTouchCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, Array())));
		sample.pointer_id = 1;
		result.push_back(event(mouse_active_tag, "topPointerCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer_payload(sample)));
	}
	for (const KeyValue<int, TouchContact> &entry : touch_contacts) {
		const TouchContact &contact = entry.value;
		PointerSample sample{ contact.tag, contact.root_position, contact.screen_position, target_origin(p_tree, contact.tag), 0, 0, "touch", entry.key, 0.0f, contact.size, contact.size, entry.key == primary_touch_id, nullptr, timestamp };
		const Dictionary touch = touch_value(sample, p_root_tag);
		result.push_back(event(contact.tag, "topTouchCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, Array())));
		result.push_back(event(contact.tag, "topPointerCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer_payload(sample)));
	}
	clear();
	return result;
}

Vector<RNNativeEvent> RNInputRouter::reconcile_tree(const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, int p_root_tag, uint64_t p_generation) {
	bool missing = mouse_active_tag != 0 && !p_registry.has_tag(mouse_active_tag);
	for (const KeyValue<int, TouchContact> &entry : touch_contacts) {
		if (!p_registry.has_tag(entry.value.tag)) {
			missing = true;
			break;
		}
	}
	return missing ? cancel_all(p_tree, p_root_tag, p_generation) : Vector<RNNativeEvent>();
}

void RNInputRouter::clear() {
	hover_tag = 0;
	mouse_active_tag = 0;
	mouse_buttons = 0;
	touch_contacts.clear();
	primary_touch_id = -1;
}
