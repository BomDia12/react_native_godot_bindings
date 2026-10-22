#include "rn_scroll_control.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/resources/texture.h"

namespace {
Rect2 viewport_for(Control *p_control, bool p_horizontal, bool p_indicator, const Size2 &p_size) {
	Ref<StyleBox> panel = p_control->get_theme_stylebox("panel", "ScrollContainer");
	Point2 origin(panel->get_margin(SIDE_LEFT), panel->get_margin(SIDE_TOP));
	Size2 size = p_size - panel->get_minimum_size();
	if (p_indicator) {
		if (p_horizontal) {
			size.y -= MAX(p_control->get_theme_stylebox("scroll", "HScrollBar")->get_minimum_size().y, p_control->get_theme_icon("increment", "HScrollBar")->get_height()) + MAX(0, p_control->get_theme_constant("padding_top", "HScrollBar")) + MAX(0, p_control->get_theme_constant("padding_bottom", "HScrollBar")) + p_control->get_theme_constant("scrollbar_v_separation", "ScrollContainer");
		} else {
			const float reserved = MAX(p_control->get_theme_stylebox("scroll", "VScrollBar")->get_minimum_size().x, p_control->get_theme_icon("increment", "VScrollBar")->get_width()) + MAX(0, p_control->get_theme_constant("padding_left", "VScrollBar")) + MAX(0, p_control->get_theme_constant("padding_right", "VScrollBar")) + p_control->get_theme_constant("scrollbar_h_separation", "ScrollContainer");
			size.x -= reserved;
			if (p_control->is_layout_rtl()) {
				origin.x += reserved;
			}
		}
	}
	return Rect2(origin, size.max(Vector2()));
}
} //namespace

RNScrollControl::RNScrollControl() {
	content = memnew(Control);
	content->set_mouse_filter(MOUSE_FILTER_PASS);
	content->set_h_size_flags(SIZE_EXPAND_FILL);
	content->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(content);
	get_h_scroll_bar()->connect("value_changed", callable_mp(this, &RNScrollControl::_scroll_changed));
	get_v_scroll_bar()->connect("value_changed", callable_mp(this, &RNScrollControl::_scroll_changed));
	connect("scroll_started", callable_mp(this, &RNScrollControl::_scroll_started));
	connect("scroll_ended", callable_mp(this, &RNScrollControl::_scroll_ended));
}
void RNScrollControl::_bind_methods() {
}
Rect2 RNScrollControl::usable_viewport() const {
	const bool horizontal = get_horizontal_scroll_mode() != SCROLL_MODE_DISABLED;
	return viewport_for(const_cast<RNScrollControl *>(this), horizontal, (horizontal ? get_horizontal_scroll_mode() : get_vertical_scroll_mode()) == SCROLL_MODE_RESERVE, get_size());
}
void RNScrollControl::set_offset(const Point2 &p_offset) {
	suppress_events = true;
	set_h_scroll(int(p_offset.x));
	set_v_scroll(int(p_offset.y));
	suppress_events = false;
}
void RNScrollControl::scroll_to(const Point2 &p_offset, bool p_animated) {
	if (scroll_tween.is_valid()) {
		scroll_tween->kill();
		scroll_tween.unref();
	}
	Point2 end(MIN(p_offset.x, MAX(0.0, get_h_scroll_bar()->get_max() - get_h_scroll_bar()->get_page())), MIN(p_offset.y, MAX(0.0, get_v_scroll_bar()->get_max() - get_v_scroll_bar()->get_page())));
	end = end.max(Point2());
	if (p_animated && is_inside_tree()) {
		scroll_tween = create_tween();
		scroll_tween->tween_property(this, NodePath("scroll_horizontal"), end.x, 0.25);
		scroll_tween->parallel()->tween_property(this, NodePath("scroll_vertical"), end.y, 0.25);
	} else {
		set_h_scroll(int(end.x));
		set_v_scroll(int(end.y));
	}
}
void RNScrollControl::restore_visible_anchor() {
	auto *child = Object::cast_to<Control>(ObjectDB::get_instance(anchor));
	if (child && content->is_ancestor_of(child)) {
		const Point2 position = content->get_global_transform().affine_inverse().xform(child->get_global_position());
		set_offset(anchor_offset + position - anchor_position);
	}
	anchor = ObjectID();
}
void RNScrollControl::refresh_content() {
	Size2 extent;
	for (int i = 0; i < content->get_child_count(); ++i) {
		auto *child = Object::cast_to<Control>(content->get_child(i));
		if (child) {
			extent = extent.max(child->get_position() + child->get_size());
		}
	}
	set_content_extent(extent);
}
void RNScrollControl::set_content_extent(const Size2 &p_extent) {
	const Size2 extent = p_extent;
	const bool changed = content->get_custom_minimum_size() != extent;
	content->set_custom_minimum_size(extent);
	content->set_size(extent.max(usable_viewport().size));
	(void)get_minimum_size();
	_reposition_children();
	get_h_scroll_bar()->set_page(usable_viewport().size.x);
	get_v_scroll_bar()->set_page(usable_viewport().size.y);
	if (changed && callbacks.content_size_changed) {
		callbacks.content_size_changed(extent);
	}
	if (callbacks.geometry_changed) {
		callbacks.geometry_changed();
	}
	if (indicators) {
		indicators->follow_scroll();
	}
}
void RNScrollControl::_notification(int p_what) {
	if ((p_what == NOTIFICATION_TRANSFORM_CHANGED || p_what == NOTIFICATION_RESIZED || p_what == NOTIFICATION_VISIBILITY_CHANGED) && indicators) {
		indicators->follow_scroll();
	}
	if (p_what == NOTIFICATION_SORT_CHILDREN) {
		get_h_scroll_bar()->set_page(usable_viewport().size.x);
		get_v_scroll_bar()->set_page(usable_viewport().size.y);
		if (callbacks.geometry_changed) {
			callbacks.geometry_changed();
		}
	}
	if (p_what == NOTIFICATION_THEME_CHANGED && callbacks.theme_changed) {
		callbacks.theme_changed();
	}
}
void RNScrollControl::_emit_scroll(const StringName &p_event) {
	if (suppress_events || !callbacks.event) {
		return;
	}
	callbacks.event(p_event);
	if (callbacks.geometry_changed) {
		callbacks.geometry_changed();
	}
	if (indicators) {
		indicators->follow_scroll();
	}
}
void RNScrollControl::_scroll_changed(double) {
	if (indicators) {
		indicators->follow_scroll();
	}
	_emit_scroll("topScroll");
}
void RNScrollControl::set_overlay_indicators(bool p_horizontal, bool p_vertical) {
	set_horizontal_scroll_mode(settings.horizontal ? SCROLL_MODE_SHOW_NEVER : SCROLL_MODE_DISABLED);
	set_vertical_scroll_mode(settings.horizontal ? SCROLL_MODE_DISABLED : SCROLL_MODE_SHOW_NEVER);
	if (!indicators) {
		indicators = memnew(RNScrollIndicators(this));
		// Drawn after the content, and clipped with it by enclosing clips.
		add_child(indicators, false, INTERNAL_MODE_BACK);
		set_notify_transform(true);
	}
	indicators->horizontal = p_horizontal;
	indicators->vertical = p_vertical;
	indicators->follow_scroll();
}
RNScrollIndicators::RNScrollIndicators(RNScrollControl *p_scroll) :
		scroll(p_scroll) {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}
void RNScrollIndicators::follow_scroll() {
	if (scroll && scroll->is_inside_tree()) {
		set_position(Point2());
		set_size(scroll->get_size());
		set_visible(scroll->is_visible_in_tree());
	}
	queue_redraw();
}
void RNScrollIndicators::_notification(int p_what) {
	// ScrollContainer fits internal children to the content too, sometimes in
	// a deferred call, so the indicators go back over the viewport after it.
	if (p_what == NOTIFICATION_RESIZED) {
		follow_scroll();
	}
	if (p_what != NOTIFICATION_DRAW || !scroll) {
		return;
	}
	constexpr real_t THICKNESS = 3;
	constexpr real_t INSET = 2;
	constexpr real_t MINIMUM_LENGTH = 16;
	const ::Color color(0.5, 0.5, 0.5, 0.75);
	const Size2 viewport = get_size();
	const Size2 extent = scroll->get_content()->get_custom_minimum_size();
	if (vertical && extent.y > viewport.y && viewport.y > 0) {
		const real_t length = MAX(MINIMUM_LENGTH, viewport.y * viewport.y / extent.y);
		const real_t travel = viewport.y - length - INSET * 2;
		const real_t y = INSET + travel * CLAMP(scroll->get_v_scroll() / (extent.y - viewport.y), 0.0, 1.0);
		draw_rect(Rect2(viewport.x - THICKNESS - INSET, y, THICKNESS, length), color);
	}
	if (horizontal && extent.x > viewport.x && viewport.x > 0) {
		const real_t length = MAX(MINIMUM_LENGTH, viewport.x * viewport.x / extent.x);
		const real_t travel = viewport.x - length - INSET * 2;
		const real_t x = INSET + travel * CLAMP(scroll->get_h_scroll() / (extent.x - viewport.x), 0.0, 1.0);
		draw_rect(Rect2(x, viewport.y - THICKNESS - INSET, length, THICKNESS), color);
	}
}
void RNScrollControl::_scroll_started() {
	dragging = true;
	if (scroll_tween.is_valid()) {
		scroll_tween->kill();
		scroll_tween.unref();
	}
	if (callbacks.drag_started) {
		callbacks.drag_started();
	}
	_emit_scroll("topScrollBeginDrag");
}
void RNScrollControl::_scroll_ended() {
	ended_during_input = true;
	if (momentum) {
		momentum = false;
		_emit_scroll("topMomentumScrollEnd");
		_snap();
	}
}
void RNScrollControl::_capture_anchor() {
	anchor = ObjectID();
	if (!settings.maintain_visible_position || content->get_child_count() == 0) {
		return;
	}
	Control *rows = Object::cast_to<Control>(content->get_child(0));
	if (!rows) {
		return;
	}
	anchor_offset = Point2(get_h_scroll(), get_v_scroll());
	const int axis = settings.horizontal ? 0 : 1;
	const int minimum = MAX(0, settings.min_index_for_visible);
	for (int i = minimum; i < rows->get_child_count(); ++i) {
		auto *child = Object::cast_to<Control>(rows->get_child(i));
		if (!child || !child->is_visible()) {
			continue;
		}
		Point2 position = content->get_global_transform().affine_inverse().xform(child->get_global_position());
		if (position[axis] + child->get_size()[axis] > anchor_offset[axis]) {
			anchor = child->get_instance_id();
			anchor_position = position;
			break;
		}
	}
}
void RNScrollControl::_snap() {
	const int axis = settings.horizontal ? 0 : 1;
	Point2 offset(get_h_scroll(), get_v_scroll());
	const double value = offset[axis];
	double target = value;
	if (!settings.snap_offsets.is_empty()) {
		target = settings.snap_offsets[0];
		for (double candidate : settings.snap_offsets) {
			if (Math::abs(candidate - value) < Math::abs(target - value)) {
				target = candidate;
			}
		}
	} else {
		const double interval = settings.snap_interval > 0 ? settings.snap_interval : settings.paging ? usable_viewport().size[axis]
																									  : 0.0;
		if (interval <= 0) {
			return;
		}
		const double free_space = usable_viewport().size[axis] - interval;
		const double shift = settings.snap_alignment == HORIZONTAL_ALIGNMENT_CENTER ? free_space / 2 : settings.snap_alignment == HORIZONTAL_ALIGNMENT_RIGHT ? free_space
																																							 : 0;
		target = Math::round((value + shift) / interval) * interval - shift;
	}
	offset[axis] = target;
	scroll_to(offset, true);
}
void RNScrollControl::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> button = p_event;
	Ref<InputEventPanGesture> pan = p_event;
	if ((button.is_valid() && button->is_pressed()) || pan.is_valid()) {
		if (scroll_tween.is_valid()) {
			scroll_tween->kill();
			scroll_tween.unref();
		}
	}
	const bool was_dragging = dragging;
	const bool drag_release = button.is_valid() && button->get_button_index() == MouseButton::LEFT && !button->is_pressed();
	ended_during_input = false;
	ScrollContainer::gui_input(p_event);
	if (was_dragging && drag_release) {
		dragging = false;
		_emit_scroll("topScrollEndDrag");
		if (ended_during_input) {
			_snap();
		} else {
			momentum = true;
			_emit_scroll("topMomentumScrollBegin");
		}
	}
}
