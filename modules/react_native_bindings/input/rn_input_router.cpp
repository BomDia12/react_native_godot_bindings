#include "rn_input_router.h"

#include "../fabric/fabric_ui_manager.h"

#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "scene/gui/control.h"

namespace {

Rect2 clipped(const Rect2 &p_left, const Rect2 &p_right) {
	return p_left.intersection(p_right);
}

Rect2 hit_rect(const RNMountedNodeSnapshot &p_node) {
	Rect2 result = p_node.has_visual_geometry ? Rect2(Point2(), p_node.local_rect.size) : p_node.root_rect;
	const Variant hit_slop_value = p_node.hit_slop;
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

RNHitTestResult RNInputRouter::hit_test_node(const RNSurfaceSnapshot &p_snapshot, int p_tag, const Point2 &p_point, const Rect2 &p_clip, bool p_include_children) {
	const RNMountedNodeSnapshot *node = p_snapshot.nodes.getptr(p_tag);
	if (!node || !node->visible || !p_clip.has_point(p_point) || node->pointer_events == "none" || (node->has_visual_geometry && !node->transform_invertible)) {
		return RNHitTestResult();
	}
	Control *control = Object::cast_to<Control>(ObjectDB::get_instance(node->object_id));
	if (control && !control->is_visible_in_tree()) {
		return RNHitTestResult();
	}
	const Point2 local_point = node->has_visual_geometry ? node->inverse_visual_transform.xform(p_point) : p_point;
	Rect2 child_clip = p_clip;
	if (node->clips_contents) {
		if (node->has_visual_geometry) {
			if (!node->viewport_rect.has_point(local_point)) {
				return RNHitTestResult();
			}
		} else {
			child_clip = clipped(child_clip, node->root_rect);
		}
	}
	const Vector<int> &children = node->has_visual_geometry ? node->paint_child_tags : node->child_tags;
	if (p_include_children && node->pointer_events != "box-only") {
		for (int i = children.size() - 1; i >= 0; --i) {
			RNHitTestResult child = hit_test_node(p_snapshot, children[i], p_point, child_clip);
			if (child.tag != 0) {
				return child;
			}
		}
	}
	if (node->self_targetable && hit_rect(*node).has_point(local_point)) {
		const Rect2 own_rect = node->has_visual_geometry ? Rect2(Point2(), node->local_rect.size) : node->root_rect;
		const RNMountedNodeSnapshot *parent = p_snapshot.nodes.getptr(node->parent_tag);
		if (!own_rect.has_point(local_point) && parent) {
			const Point2 parent_point = parent->has_visual_geometry ? parent->inverse_visual_transform.xform(p_point) : p_point;
			if (!(parent->has_visual_geometry ? Rect2(Point2(), parent->local_rect.size) : parent->root_rect).has_point(parent_point)) {
				return RNHitTestResult();
			}
		}
		if (!node->span_rects.is_empty()) {
			bool inside_span = false;
			for (const Rect2 &rect : node->span_rects) {
				inside_span = inside_span || rect.has_point(local_point);
			}
			if (!inside_span) {
				return RNHitTestResult();
			}
		}
		return RNHitTestResult{ node->tag, node->root_rect.position };
	}
	return RNHitTestResult();
}

RNHitTestResult RNInputRouter::hit_test(const RNSurfaceSnapshot &p_snapshot, const Point2 &p_point) {
	const RNMountedNodeSnapshot *root = p_snapshot.nodes.getptr(p_snapshot.root_tag);
	if (!root) {
		return RNHitTestResult();
	}
	const Vector<int> &children = root->has_visual_geometry ? root->paint_child_tags : root->child_tags;
	for (int i = children.size() - 1; i >= 0; --i) {
		RNHitTestResult result = hit_test_node(p_snapshot, children[i], p_point, root->root_rect);
		if (result.tag != 0) {
			return result;
		}
	}
	return RNHitTestResult();
}

Point2 RNInputRouter::target_origin(const RNSurfaceSnapshot &p_snapshot, int p_tag) {
	const RNMountedNodeSnapshot *node = p_snapshot.nodes.getptr(p_tag);
	return node ? node->root_rect.position : Point2();
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

void RNInputRouter::append_mouse_hover(RouteResult &r_result, const RNSurfaceSnapshot &p_snapshot, const RNHitTestResult &p_hit, const Point2 &p_root_position, const Point2 &p_screen_position, const InputEventWithModifiers *p_modifiers, uint64_t p_generation, uint64_t p_timestamp) {
	const int next_hover = p_hit.tag;
	if (next_hover == hover_tag) {
		return;
	}

	if (hover_tag != 0) {
		const Dictionary payload = pointer_payload({ hover_tag, p_root_position, p_screen_position, target_origin(p_snapshot, hover_tag), -1, mouse_buttons, "mouse", 1, 0.0f, 1.0f, 1.0f, true, p_modifiers, p_timestamp });
		r_result.events.push_back(event(hover_tag, "topPointerOut", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
		r_result.events.push_back(event(hover_tag, "topPointerLeave", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
	}
	if (next_hover != 0) {
		const Dictionary payload = pointer_payload({ next_hover, p_root_position, p_screen_position, p_hit.root_origin, -1, mouse_buttons, "mouse", 1, mouse_buttons ? 0.5f : 0.0f, 1.0f, 1.0f, true, p_modifiers, p_timestamp });
		r_result.events.push_back(event(next_hover, "topPointerOver", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
		r_result.events.push_back(event(next_hover, "topPointerEnter", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
	}
	hover_tag = next_hover;
}

Array RNInputRouter::current_touches(const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_timestamp) const {
	Array touches;
	for (const KeyValue<int, TouchContact> &entry : touch_contacts) {
		const TouchContact &contact = entry.value;
		PointerSample sample{ contact.tag, contact.root_position, contact.screen_position, target_origin(p_snapshot, contact.tag), -1, 0, "touch", entry.key, contact.pressure, contact.size, contact.size, entry.key == primary_touch_id, nullptr, p_timestamp };
		touches.push_back(touch_value(sample, p_root_tag));
	}
	return touches;
}

RNInputRouter::RouteResult RNInputRouter::route_pointer(const Ref<InputEvent> &p_event, const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_generation, const Point2 &p_root_position, const Point2 &p_screen_position, int p_native_tag) {
	RouteResult result = route_pointer_impl(p_event, p_snapshot, p_root_tag, p_generation, p_root_position, p_screen_position, p_native_tag);
	auto localize = [&](Dictionary &payload) {
		const int tag = payload.get("target", 0);
		const RNMountedNodeSnapshot *node = p_snapshot.nodes.getptr(tag);
		if (!node || !node->has_visual_geometry || !node->transform_invertible || !payload.has("pageX")) {
			return;
		}
		const Point2 point = node->inverse_visual_transform.xform(Point2(real_t(payload["pageX"]), real_t(payload["pageY"])));
		if (payload.has("offsetX")) {
			payload["offsetX"] = point.x;
			payload["offsetY"] = point.y;
		}
		if (payload.has("locationX")) {
			payload["locationX"] = point.x;
			payload["locationY"] = point.y;
		}
	};
	for (RNNativeEvent &event : result.events) {
		localize(event.payload);
		for (const char *field : { "touches", "changedTouches" }) {
			if (!event.payload.has(field)) {
				continue;
			}
			Array touches = event.payload[field];
			for (int i = 0; i < touches.size(); ++i) {
				Dictionary touch = touches[i];
				localize(touch);
			}
		}
	}
	return result;
}

RNInputRouter::RouteResult RNInputRouter::route_pointer_impl(const Ref<InputEvent> &p_event, const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_generation, const Point2 &p_root_position, const Point2 &p_screen_position, int p_native_tag) {
	RouteResult result;
	const uint64_t timestamp = timestamp_now();
	RNHitTestResult hit = p_native_tag < 0 ? hit_test(p_snapshot, p_root_position) : RNHitTestResult();
	for (int native_tag = p_native_tag; native_tag > 0 && hit.tag == 0;) {
		hit = hit_test_node(p_snapshot, native_tag, p_root_position, Rect2(Point2(-1e9, -1e9), Size2(2e9, 2e9)), native_tag == p_native_tag);
		const auto *node = p_snapshot.nodes.getptr(native_tag);
		native_tag = node ? node->parent_tag : 0;
	}

	if (Ref<InputEventMouseMotion> motion = p_event; motion.is_valid()) {
		mouse_root_position = p_root_position;
		mouse_screen_position = p_screen_position;
		append_mouse_hover(result, p_snapshot, hit, p_root_position, p_screen_position, motion.ptr(), p_generation, timestamp);
		const int target = mouse_active_tag != 0 ? mouse_active_tag : hit.tag;
		if (target != 0) {
			const Point2 origin = target == hit.tag ? hit.root_origin : target_origin(p_snapshot, target);
			PointerSample sample{ target, p_root_position, p_screen_position, origin, -1, mouse_buttons, "mouse", 1, mouse_buttons ? 0.5f : 0.0f, 1.0f, 1.0f, true, motion.ptr(), timestamp };
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

		const int target = button == 0 && !button_event->is_pressed() && mouse_active_tag != 0 ? mouse_active_tag : hit.tag;
		if (target == 0) {
			if (button == 0 && !button_event->is_pressed()) {
				mouse_active_tag = 0;
			}
			return result;
		}

		const Point2 origin = target == hit.tag ? hit.root_origin : target_origin(p_snapshot, target);
		PointerSample sample{ target, p_root_position, p_screen_position, origin, button, mouse_buttons, "mouse", 1, button_event->is_pressed() ? 0.5f : 0.0f, 1.0f, 1.0f, true, button_event.ptr(), timestamp };
		if (button_event->is_pressed()) {
			const Dictionary payload = pointer_payload(sample);
			result.events.push_back(event(target, "topPointerDown", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, payload));
			if (button != 0) {
				result.events.push_back(event(target, button == 1 ? "topMiddleClick" : "topRightClick", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, payload));
			}
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
				if (button == 0 && hit.tag == mouse_active_tag) {
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
			const int target = hit.tag;
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
			PointerSample sample{ target, p_root_position, p_screen_position, hit.root_origin, 0, 1, "touch", index, 0.5f, 1.0f, 1.0f, index == primary_touch_id, nullptr, timestamp };
			const Dictionary pointer = pointer_payload(sample);
			result.events.push_back(event(target, "topPointerDown", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer));
			const Dictionary touch = touch_value(sample, p_root_tag);
			result.events.push_back(event(target, "topTouchStart", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, current_touches(p_snapshot, p_root_tag, timestamp))));
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
		PointerSample sample{ ended.tag, p_root_position, p_screen_position, target_origin(p_snapshot, ended.tag), 0, 0, "touch", index, 0.0f, 1.0f, 1.0f, was_primary, nullptr, timestamp };
		const Dictionary touch = touch_value(sample, p_root_tag);
		const Dictionary payload = touch_payload(touch, current_touches(p_snapshot, p_root_tag, timestamp));
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
		PointerSample sample{ target, p_root_position, p_screen_position, target_origin(p_snapshot, target), -1, 1, "touch", drag->get_index(), contact->pressure, 1.0f, 1.0f, drag->get_index() == primary_touch_id, nullptr, timestamp };
		result.events.push_back(event(target, "topPointerMove", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, pointer_payload(sample)));
		const Dictionary touch = touch_value(sample, p_root_tag);
		result.events.push_back(event(target, "topTouchMove", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, touch_payload(touch, current_touches(p_snapshot, p_root_tag, timestamp))));
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

Vector<RNNativeEvent> RNInputRouter::cancel_all(const RNSurfaceSnapshot *p_snapshot, int p_root_tag, uint64_t p_generation) {
	Vector<RNNativeEvent> result;
	const uint64_t timestamp = timestamp_now();
	if (hover_tag != 0) {
		const Dictionary payload = pointer_payload({ hover_tag, mouse_root_position, mouse_screen_position, p_snapshot ? target_origin(*p_snapshot, hover_tag) : Point2(), -1, mouse_buttons, "mouse", 1, mouse_buttons ? 0.5f : 0.0f, 1.0f, 1.0f, true, nullptr, timestamp });
		result.push_back(event(hover_tag, "topPointerOut", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
		result.push_back(event(hover_tag, "topPointerLeave", FabricUIManager::EVENT_PRIORITY_CONTINUOUS, p_generation, payload));
	}
	if (mouse_active_tag != 0) {
		const Point2 origin = p_snapshot ? target_origin(*p_snapshot, mouse_active_tag) : Point2();
		PointerSample sample{ mouse_active_tag, mouse_root_position, mouse_screen_position, origin, 0, 0, "mouse", 0, 0.0f, 1.0f, 1.0f, true, nullptr, timestamp };
		const Dictionary touch = touch_value(sample, p_root_tag);
		result.push_back(event(mouse_active_tag, "topTouchCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, Array())));
		sample.pointer_id = 1;
		result.push_back(event(mouse_active_tag, "topPointerCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer_payload(sample)));
	}
	for (const KeyValue<int, TouchContact> &entry : touch_contacts) {
		const TouchContact &contact = entry.value;
		PointerSample sample{ contact.tag, contact.root_position, contact.screen_position, p_snapshot ? target_origin(*p_snapshot, contact.tag) : Point2(), 0, 0, "touch", entry.key, 0.0f, contact.size, contact.size, entry.key == primary_touch_id, nullptr, timestamp };
		const Dictionary touch = touch_value(sample, p_root_tag);
		result.push_back(event(contact.tag, "topTouchCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, touch_payload(touch, Array())));
		result.push_back(event(contact.tag, "topPointerCancel", FabricUIManager::EVENT_PRIORITY_DISCRETE, p_generation, pointer_payload(sample)));
	}
	if (p_snapshot) {
		auto localize = [&](Dictionary &payload) {
			const RNMountedNodeSnapshot *node = p_snapshot->nodes.getptr(int(payload.get("target", 0)));
			if (!node || !node->has_visual_geometry || !node->transform_invertible || !payload.has("pageX")) {
				return;
			}
			const Point2 position = node->inverse_visual_transform.xform(Point2(payload["pageX"], payload["pageY"]));
			if (payload.has("offsetX")) {
				payload["offsetX"] = position.x;
				payload["offsetY"] = position.y;
			}
			if (payload.has("locationX")) {
				payload["locationX"] = position.x;
				payload["locationY"] = position.y;
			}
		};
		for (RNNativeEvent &event : result) {
			localize(event.payload);
			for (const char *key : { "touches", "changedTouches" }) {
				const Array touches = event.payload.get(key, Array());
				for (const Variant &value : touches) {
					Dictionary touch = value;
					localize(touch);
				}
			}
		}
	}
	clear();
	return result;
}

Vector<RNNativeEvent> RNInputRouter::reconcile_snapshot(const RNSurfaceSnapshot *p_old_snapshot, const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_generation) {
	auto eligible = [&](int tag) {
		const RNMountedNodeSnapshot *target = p_snapshot.nodes.getptr(tag);
		if (!target || !target->self_targetable) {
			return false;
		}
		while (target) {
			if (!target->visible || target->pointer_events == "none" || (target->has_visual_geometry && !target->transform_invertible)) {
				return false;
			}
			const RNMountedNodeSnapshot *parent = p_snapshot.nodes.getptr(target->parent_tag);
			if (parent && parent->pointer_events == "box-only") {
				return false;
			}
			target = parent;
		}
		return true;
	};
	bool missing = hover_tag != 0 && !eligible(hover_tag);
	missing = missing || (mouse_active_tag != 0 && !eligible(mouse_active_tag));
	for (const KeyValue<int, TouchContact> &entry : touch_contacts) {
		if (!eligible(entry.value.tag)) {
			missing = true;
			break;
		}
	}
	if (!missing) {
		RouteResult hover;
		if (hover_tag && hit_test_node(p_snapshot, hover_tag, mouse_root_position, Rect2(-1e8, -1e8, 2e8, 2e8)).tag != hover_tag) {
			append_mouse_hover(hover, p_snapshot, RNHitTestResult(), mouse_root_position, mouse_screen_position, nullptr, p_generation, timestamp_now());
		}
		return hover.events;
	}
	Vector<RNNativeEvent> events = cancel_all(p_old_snapshot, p_root_tag, p_generation);
	if (p_old_snapshot) {
		for (RNNativeEvent &event : events) {
			const RNMountedNodeSnapshot *old_node = p_old_snapshot->nodes.getptr(event.tag);
			if (old_node && old_node->shadow_node.is_valid()) {
				event.retained_target = old_node->shadow_node->event_target;
			}
		}
	}
	return events;
}

void RNInputRouter::clear() {
	hover_tag = 0;
	mouse_active_tag = 0;
	mouse_buttons = 0;
	touch_contacts.clear();
	primary_touch_id = -1;
}
