#pragma once

#include "../fabric/rn_native_event.h"
#include "../fabric/rn_registry.h"

#include "core/input/input_event.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class RNInputRouter {
public:
	struct RouteResult {
		Vector<RNNativeEvent> events;
		int focus_tag = 0;
		bool accepted = false;
	};

private:
	struct TouchContact {
		int tag = 0;
		Point2 root_position;
		Point2 screen_position;
		float pressure = 0.5f;
		float size = 1.0f;
	};
	struct PointerSample {
		int tag = 0;
		Point2 root_position;
		Point2 screen_position;
		Point2 target_origin;
		int button = -1;
		int buttons = 0;
		String pointer_type;
		int pointer_id = 0;
		float pressure = 0.0f;
		float width = 1.0f;
		float height = 1.0f;
		bool primary = false;
		const InputEventWithModifiers *modifiers = nullptr;
		uint64_t timestamp = 0;
	};

	int hover_tag = 0;
	int mouse_active_tag = 0;
	int mouse_buttons = 0;
	Point2 mouse_root_position;
	Point2 mouse_screen_position;
	HashMap<int, TouchContact> touch_contacts;
	int primary_touch_id = -1;

	static int hit_test_node(const Ref<RNShadowNode> &p_node, const Point2 &p_point, const Point2 &p_parent_origin, const Rect2 &p_clip, const RNRegistry *p_registry);
	static bool find_origin(const Ref<RNShadowNode> &p_node, int p_tag, const Point2 &p_parent_origin, Point2 &r_origin);
	static Dictionary pointer_payload(const PointerSample &p_sample);
	static Dictionary touch_value(const PointerSample &p_sample, int p_root_tag);
	static Dictionary touch_payload(const Dictionary &p_touch, const Array &p_touches);
	static RNNativeEvent event(int p_tag, const String &p_name, int p_priority, uint64_t p_generation, const Dictionary &p_payload);
	static int mouse_button(MouseButton p_button);
	static int mouse_button_mask(MouseButton p_button);
	static String key_name(const Ref<InputEventKey> &p_key);
	static String code_name(const Ref<InputEventKey> &p_key);

	int hit_test(const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, const Size2 &p_root_size, const Point2 &p_point) const;
	Point2 target_origin(const Ref<RNShadowNode> &p_tree, int p_tag) const;
	Array current_touches(const Ref<RNShadowNode> &p_tree, int p_root_tag, uint64_t p_timestamp) const;
	void append_mouse_hover(RouteResult &r_result, const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, const Size2 &p_root_size, const Point2 &p_root_position, const Point2 &p_screen_position, const InputEventWithModifiers *p_modifiers, uint64_t p_generation, uint64_t p_timestamp);

public:
	static int hit_test(const Ref<RNShadowNode> &p_tree, const Size2 &p_root_size, const Point2 &p_point);

	RouteResult route_pointer(const Ref<InputEvent> &p_event, const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, const Size2 &p_root_size, int p_root_tag, uint64_t p_generation, const Point2 &p_root_position, const Point2 &p_screen_position);
	RouteResult route_key(const Ref<InputEventKey> &p_key, int p_target_tag, uint64_t p_generation);
	Vector<RNNativeEvent> reconcile_tree(const Ref<RNShadowNode> &p_tree, const RNRegistry &p_registry, int p_root_tag, uint64_t p_generation);
	Vector<RNNativeEvent> cancel_all(const Ref<RNShadowNode> &p_tree, int p_root_tag, uint64_t p_generation);
	void clear();
};
