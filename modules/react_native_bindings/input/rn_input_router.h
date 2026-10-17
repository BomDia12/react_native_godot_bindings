#pragma once

#include "../mounting/rn_component_view.h"

#include "core/input/input_event.h"
#include "core/templates/hash_map.h"

// Translates Godot input on the control Godot picked into React Native touch,
// pointer and key events, sent through the target's upstream event emitter.
class RNInputRouter {
public:
	struct Target {
		facebook::react::Tag tag = 0;
		std::shared_ptr<const facebook::react::TouchEventEmitter> emitter;
		ObjectID control;
		ObjectID focus;
		bool owns_activation = false;

		bool is_valid() const { return emitter != nullptr; }
	};

	// One pointer position: in the root's coordinates, on the screen, and in the
	// viewport of the control that received it.
	struct Position {
		Point2 page;
		Point2 screen;
		Point2 viewport;
	};

private:
	struct Contact {
		Target target;
		Position position;
		float pressure = 0.5f;
	};

	Target hover;
	Target mouse_active;
	int mouse_buttons = 0;
	Position mouse_position;
	HashMap<int, Contact> touches;
	int primary_touch = -1;

	static facebook::react::Point point(const Point2 &p_point);
	static Point2 offset_in(const Target &p_target, const Position &p_position);
	static facebook::react::Touch touch(const Target &p_target, const Position &p_position, int p_identifier, float p_force);
	static facebook::react::PointerEvent pointer(const Target &p_target, const Position &p_position, int p_pointer_id, const char *p_type, int p_button, int p_buttons, float p_pressure, bool p_primary, const InputEventWithModifiers *p_modifiers);
	facebook::react::TouchEvent touch_event(const facebook::react::Touch &p_changed, bool p_ended) const;

	void _hover(const Target &p_target, const Position &p_position, const InputEventWithModifiers *p_modifiers);
	void _mouse_motion(const Ref<InputEventMouseMotion> &p_motion, const Target &p_target, const Position &p_position);
	Control *_mouse_button(const Ref<InputEventMouseButton> &p_button, const Target &p_target, const Position &p_position);
	Control *_screen_touch(const Ref<InputEventScreenTouch> &p_touch, const Target &p_target, const Position &p_position);
	void _screen_drag(const Ref<InputEventScreenDrag> &p_drag, const Position &p_position);
	void _cancel(const Target &p_target, const Position &p_position, int p_touch, int p_pointer, const char *p_type, bool p_primary);

public:
	static Target target_for(const RNComponentView *p_view, const Point2 &p_local);

	// Returns the pressed view's focus control after a primary press.
	Control *route_pointer(const Ref<InputEvent> &p_event, const Target &p_target, const Position &p_position);
	void route_key(const Ref<InputEventKey> &p_key, const Target &p_target);
	void view_destroyed(facebook::react::Tag p_tag);
	void cancel_all();
};
