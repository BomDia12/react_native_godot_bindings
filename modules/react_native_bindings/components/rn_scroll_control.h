#pragma once
#include "rn_host_descriptor.h"

#include "scene/animation/tween.h"
#include "scene/gui/scroll_container.h"

struct RNScrollData : RNComponentData {
	bool horizontal = false;
	bool indicator = true;
	Rect2 usable_viewport;
};

class RNScrollControl : public ScrollContainer {
	GDCLASS(RNScrollControl, ScrollContainer);
	Control *content = nullptr;
	RNHostContext published_context;
	Dictionary published_props;
	Ref<Tween> scroll_tween;
	Point2 last_offset;
	bool suppress_events = false;
	bool dragging = false;
	bool momentum = false;
	ObjectID anchor;
	Point2 anchor_position;
	Point2 anchor_offset;
	void _capture_anchor();
	void _snap();
	void _scroll_changed(double p_value);
	void _scroll_started();
	void _scroll_ended();
	void _emit_scroll(const StringName &p_event);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	RNScrollControl();
	Control *get_content() const { return content; }
	Rect2 usable_viewport() const;
	void publish(const RNPreparedHostState &p_state, const RNHostContext &p_context);
	void refresh_content();
	void scroll_to(const Point2 &p_offset, bool p_animated);
	void set_offset(const Point2 &p_offset);
	void prepare_anchor() { _capture_anchor(); }
};
std::shared_ptr<const RNHostDescriptor> rn_scroll_descriptor(const StringName &p_name = "RCTScrollView", bool p_horizontal = false);
std::shared_ptr<const RNHostDescriptor> rn_scroll_content_descriptor(const StringName &p_name);
