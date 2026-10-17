#pragma once
#include "scene/animation/tween.h"
#include "scene/gui/control.h"
#include "scene/gui/panel.h"
#include "scene/gui/scroll_container.h"

#include <functional>
#include <memory>

struct RNScrollSettings {
	bool horizontal = false;
	bool paging = false;
	double snap_interval = 0;
	Vector<double> snap_offsets;
	HorizontalAlignment snap_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	bool maintain_visible_position = false;
	int min_index_for_visible = 0;
};
struct RNScrollCallbacks {
	std::function<void(const StringName &)> event;
	std::function<void(const Size2 &)> content_size_changed;
	std::function<void()> geometry_changed;
	std::function<void()> theme_changed;
	std::function<void()> drag_started;
};
class RNScrollControl;

// React Native draws scroll indicators over the content instead of reserving
// layout space, so this top-level child paints them above the ScrollContainer.
class RNScrollIndicators : public Control {
	GDCLASS(RNScrollIndicators, Control);
	RNScrollControl *scroll = nullptr;

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	explicit RNScrollIndicators(RNScrollControl *p_scroll = nullptr);
	bool horizontal = false;
	bool vertical = false;
	void follow_scroll();
};

class RNScrollControl : public ScrollContainer {
	GDCLASS(RNScrollControl, ScrollContainer);
	Control *content = nullptr;
	RNScrollIndicators *indicators = nullptr;
	RNScrollSettings settings;
	RNScrollCallbacks callbacks;
	Variant published_offset;
	Ref<Tween> scroll_tween;
	Point2 last_offset;
	bool suppress_events = false;
	bool dragging = false;
	bool momentum = false;
	bool ended_during_input = false;
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
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNScrollControl();
	Control *get_content() const { return content; }
	Rect2 usable_viewport() const;
	void set_settings(const RNScrollSettings &p_settings) { settings = p_settings; }
	void set_callbacks(RNScrollCallbacks p_callbacks) { callbacks = std::move(p_callbacks); }
	void restore_visible_anchor();
	void set_overlay_indicators(bool p_horizontal, bool p_vertical);
	void set_content_extent(const Size2 &p_extent);
	void refresh_content();
	void scroll_to(const Point2 &p_offset, bool p_animated);
	void set_offset(const Point2 &p_offset);
	void prepare_anchor() { _capture_anchor(); }
};
class RNScrollContentControl : public Panel {
	GDCLASS(RNScrollContentControl, Panel);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};
