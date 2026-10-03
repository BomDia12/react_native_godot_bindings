#pragma once

#include "../fabric/rn_native_event.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "core/input/input_event.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

struct RNHitTestResult {
	int tag = 0;
	Point2 root_origin;
};

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

	static RNHitTestResult hit_test_node(const RNSurfaceSnapshot &p_snapshot, int p_tag, const Point2 &p_point, const Rect2 &p_clip, bool p_include_children = true);
	static Point2 target_origin(const RNSurfaceSnapshot &p_snapshot, int p_tag);
	static Dictionary pointer_payload(const PointerSample &p_sample);
	static Dictionary touch_value(const PointerSample &p_sample, int p_root_tag);
	static Dictionary touch_payload(const Dictionary &p_touch, const Array &p_touches);
	static RNNativeEvent event(int p_tag, const String &p_name, int p_priority, uint64_t p_generation, const Dictionary &p_payload);
	static int mouse_button(MouseButton p_button);
	static int mouse_button_mask(MouseButton p_button);
	static String key_name(const Ref<InputEventKey> &p_key);
	static String code_name(const Ref<InputEventKey> &p_key);

	RouteResult route_pointer_impl(const Ref<InputEvent> &p_event, const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_generation, const Point2 &p_root_position, const Point2 &p_screen_position, int p_native_tag = -1);
	Array current_touches(const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_timestamp) const;
	void append_mouse_hover(RouteResult &r_result, const RNSurfaceSnapshot &p_snapshot, const RNHitTestResult &p_hit, const Point2 &p_root_position, const Point2 &p_screen_position, const InputEventWithModifiers *p_modifiers, uint64_t p_generation, uint64_t p_timestamp);

public:
	static RNHitTestResult hit_test(const RNSurfaceSnapshot &p_snapshot, const Point2 &p_point);

	RouteResult route_pointer(const Ref<InputEvent> &p_event, const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_generation, const Point2 &p_root_position, const Point2 &p_screen_position, int p_native_tag = -1);
	RouteResult route_key(const Ref<InputEventKey> &p_key, int p_target_tag, uint64_t p_generation);
	Vector<RNNativeEvent> reconcile_snapshot(const RNSurfaceSnapshot *p_old_snapshot, const RNSurfaceSnapshot &p_snapshot, int p_root_tag, uint64_t p_generation);
	Vector<RNNativeEvent> cancel_all(const RNSurfaceSnapshot *p_snapshot, int p_root_tag, uint64_t p_generation);
	void clear();
};
