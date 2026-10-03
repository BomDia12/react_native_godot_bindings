#include "rn_scroll_control.h"

#include "../fabric/rn_view_style.h"
#include "rn_view_control.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/resources/texture.h"

namespace {
Dictionary scroll_config() {
	Dictionary events;
	for (const char *name : { "Scroll", "ScrollBeginDrag", "ScrollEndDrag", "MomentumScrollBegin", "MomentumScrollEnd", "ContentSizeChange" }) {
		Dictionary registration;
		registration["registrationName"] = String("on") + name;
		events[String("top") + name] = registration;
	}
	Dictionary config;
	config["directEventTypes"] = events;
	return config;
}
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
class RNScrollDescriptor : public RNHostDescriptor {
	bool horizontal_default;

public:
	RNScrollDescriptor(const StringName &p_name, bool p_horizontal) :
			RNHostDescriptor(p_name, RNHostTraits{ true, true, true, false, false, false, true, true, true, true, true }, scroll_config()), horizontal_default(p_horizontal) {}
	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		auto data = std::make_shared<RNScrollData>();
		data->horizontal = r_state.props.get("horizontal", horizontal_default);
		data->indicator = r_state.props.get(data->horizontal ? "showsHorizontalScrollIndicator" : "showsVerticalScrollIndicator", true);
		for (const char *key : { "contentOffset", "contentInset" }) {
			if (r_state.props.has(key) && r_state.props[key].get_type() != Variant::DICTIONARY) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "Scroll offset/insets require a record", "scroll.prepare");
				return false;
			}
		}
		r_state.component_data = data;
		return true;
	}
	bool resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &) const override;
	bool owns_input_control(Control *p_host, Control *p_control) const override {
		auto *scroll = Object::cast_to<RNScrollControl>(p_host);
		return p_control == scroll || p_control == scroll->get_content() || p_control == scroll->get_h_scroll_bar() || p_control == scroll->get_v_scroll_bar();
	}
	Control *create_host(const RNHostContext &) const override { return memnew(RNScrollControl); }
	Control *get_child_container(Control *p_host, const RNHostContext &) const override { return Object::cast_to<RNScrollControl>(p_host)->get_content(); }
	RNChildLayoutPolicy get_child_layout_policy(const RNPreparedHostState &p_state) const override { return std::static_pointer_cast<const RNScrollData>(p_state.component_data)->horizontal ? RNChildLayoutPolicy::SCROLL_HORIZONTAL : RNChildLayoutPolicy::SCROLL_VERTICAL; }
	Rect2 get_child_layout_viewport(const RNPreparedHostState &p_state, const Size2 &p_size) const override {
		const auto data = std::static_pointer_cast<const RNScrollData>(p_state.component_data);
		return Rect2(data->usable_viewport.position, (p_size - data->usable_viewport.size).max(Vector2()));
	}
	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		auto *scroll = Object::cast_to<RNScrollControl>(p_host);
		const auto data = std::static_pointer_cast<const RNScrollData>(p_state.component_data);
		const bool enabled = p_state.props.get("scrollEnabled", true);
		const auto mode = !enabled ? ScrollContainer::SCROLL_MODE_DISABLED : data->indicator ? ScrollContainer::SCROLL_MODE_RESERVE
																							 : ScrollContainer::SCROLL_MODE_SHOW_NEVER;
		scroll->set_horizontal_scroll_mode(data->horizontal ? mode : ScrollContainer::SCROLL_MODE_DISABLED);
		scroll->set_vertical_scroll_mode(data->horizontal ? ScrollContainer::SCROLL_MODE_DISABLED : mode);
		scroll->set_follow_focus(true);
		scroll->set_mouse_filter(p_state.branch_targetable ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_IGNORE);
		scroll->set_visible(String(p_state.props.get("display", "flex")) != "none");
		scroll->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
		return true;
	}
	Variant capture_state(Control *p_host) const override {
		auto *scroll = Object::cast_to<RNScrollControl>(p_host);
		scroll->prepare_anchor();
		Dictionary state;
		state["offset"] = Point2(scroll->get_h_scroll(), scroll->get_v_scroll());
		state["hmode"] = int(scroll->get_horizontal_scroll_mode());
		state["vmode"] = int(scroll->get_vertical_scroll_mode());
		state["visible"] = scroll->is_visible();
		state["modulate"] = scroll->get_modulate();
		state["content_min"] = scroll->get_content()->get_custom_minimum_size();
		return state;
	}
	void restore_state(Control *p_host, const Variant &p_state) const override {
		auto *scroll = Object::cast_to<RNScrollControl>(p_host);
		Dictionary state = p_state;
		scroll->set_horizontal_scroll_mode(ScrollContainer::ScrollMode(int(state["hmode"])));
		scroll->set_vertical_scroll_mode(ScrollContainer::ScrollMode(int(state["vmode"])));
		scroll->set_visible(state["visible"]);
		scroll->set_modulate(state["modulate"]);
		scroll->get_content()->set_custom_minimum_size(state["content_min"]);
		scroll->set_offset(state["offset"]);
	}
	void after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const override { Object::cast_to<RNScrollControl>(p_host)->publish(p_state, p_context); }
	RNHostGeometry read_geometry(Control *p_host, const RNHostContext &p_context) const override {
		auto geometry = RNHostDescriptor::read_geometry(p_host, p_context);
		auto *scroll = Object::cast_to<RNScrollControl>(p_host);
		geometry.viewport = scroll->usable_viewport();
		geometry.clips_contents = true;
		geometry.scroll_offset = Point2(scroll->get_h_scroll(), scroll->get_v_scroll());
		geometry.content_size = scroll->get_content()->get_size();
		return geometry;
	}
	bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const override {
		auto *scroll = Object::cast_to<RNScrollControl>(p_host);
		Array args = p_arguments;
		if (p_command == "scrollTo" && args.size() == 3) {
			scroll->scroll_to(Point2(args[0], args[1]), args[2]);
			return true;
		}
		if (p_command == "scrollToEnd" && args.size() == 1) {
			scroll->scroll_to(scroll->get_content()->get_size(), args[0]);
			return true;
		}
		if (p_command == "flashScrollIndicators") {
			scroll->get_h_scroll_bar()->queue_redraw();
			scroll->get_v_scroll_bar()->queue_redraw();
			return true;
		}
		return RNHostDescriptor::dispatch_command(p_host, p_command, p_arguments, p_context, r_error);
	}
};
class RNScrollContentDescriptor : public RNHostDescriptor {
public:
	RNScrollContentDescriptor(const StringName &p_name) :
			RNHostDescriptor(p_name, RNHostTraits{ true, true, true, false, false, false, false, false, true, true, true }) {}
	Control *create_host(const RNHostContext &) const override {
		auto *control = memnew(Panel);
		control->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		return control;
	}
	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		auto *panel = Object::cast_to<Panel>(p_host);
		panel->set_visible(String(p_state.props.get("display", "flex")) != "none");
		panel->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
		panel->add_theme_style_override("panel", RNViewStyle::build_stylebox(p_state.props, p_state.layout_rtl));
		panel->set_mouse_filter(p_state.branch_targetable ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
		return true;
	}
};
} //namespace

#include "../root_view/react_native_root_view.h"
bool RNScrollDescriptor::resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &) const {
	auto data = std::make_shared<RNScrollData>(*std::static_pointer_cast<const RNScrollData>(r_state.component_data));
	Control *theme = p_context.owner;
	Control *scratch = nullptr;
	if (!theme) {
		scratch = memnew(ScrollContainer);
		theme = scratch;
	}
	const Rect2 usable = viewport_for(theme, data->horizontal, data->indicator, Size2(100, 100));
	data->usable_viewport = Rect2(usable.position, Size2(100, 100) - usable.size);
	r_state.component_data = data;
	r_state.dependency_revision = uint64_t(data->usable_viewport.size.x * 4096 + data->usable_viewport.size.y) ^ (p_context.owner ? p_context.owner->get_native_resource_revision() : 0);
	if (scratch) {
		memdelete(scratch);
	}
	return true;
}
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
void RNScrollControl::publish(const RNPreparedHostState &p_state, const RNHostContext &p_context) {
	const bool offset_changed = published_props.get("contentOffset", Variant()) != p_state.props.get("contentOffset", Variant());
	published_context = p_context;
	published_props = p_state.props.duplicate(true);
	refresh_content();
	if (!offset_changed && anchor.is_valid()) {
		auto *child = Object::cast_to<Control>(ObjectDB::get_instance(anchor));
		if (child && content->is_ancestor_of(child)) {
			const Point2 position = content->get_global_transform().affine_inverse().xform(child->get_global_position());
			set_offset(anchor_offset + position - anchor_position);
		}
	}
	anchor = ObjectID();
	if (offset_changed && published_props.has("contentOffset")) {
		const Dictionary offset = published_props["contentOffset"];
		scroll_to(Point2(offset.get("x", 0), offset.get("y", 0)), false);
	}
}
void RNScrollControl::refresh_content() {
	Size2 extent;
	for (int i = 0; i < content->get_child_count(); ++i) {
		auto *child = Object::cast_to<Control>(content->get_child(i));
		if (child) {
			extent = extent.max(child->get_position() + child->get_size());
		}
	}
	const bool changed = content->get_custom_minimum_size() != extent;
	content->set_custom_minimum_size(extent);
	content->set_size(extent.max(usable_viewport().size));
	(void)get_minimum_size();
	_reposition_children();
	get_h_scroll_bar()->set_page(usable_viewport().size.x);
	get_v_scroll_bar()->set_page(usable_viewport().size.y);
	if (changed && published_context.event_sink && published_context.event_sink->emit) {
		Dictionary event;
		event["width"] = extent.x;
		event["height"] = extent.y;
		published_context.event_sink->emit(published_context.tag, "topContentSizeChange", event, published_context.revision);
	}
	if (published_context.event_sink && published_context.event_sink->invalidate_geometry) {
		published_context.event_sink->invalidate_geometry();
	}
}
void RNScrollControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		if (dragging && !is_drag_scrolling()) {
			dragging = false;
			_emit_scroll("topScrollEndDrag");
		}
		if (is_momentum_scrolling() && !momentum) {
			momentum = true;
			_emit_scroll("topMomentumScrollBegin");
		}
		if (momentum && !is_momentum_scrolling()) {
			momentum = false;
			_emit_scroll("topMomentumScrollEnd");
		}
	}
	if (p_what == NOTIFICATION_SORT_CHILDREN) {
		get_h_scroll_bar()->set_page(usable_viewport().size.x);
		get_v_scroll_bar()->set_page(usable_viewport().size.y);
	}
	if (p_what == NOTIFICATION_THEME_CHANGED && published_context.event_sink && published_context.event_sink->invalidate_layout) {
		published_context.event_sink->invalidate_layout();
	}
	if (p_what == NOTIFICATION_SORT_CHILDREN && published_context.event_sink && published_context.event_sink->invalidate_geometry) {
		published_context.event_sink->invalidate_geometry();
	}
}
void RNScrollControl::_emit_scroll(const StringName &p_event) {
	if (!published_context.event_sink || !published_context.event_sink->emit || suppress_events) {
		return;
	}
	Dictionary offset;
	offset["x"] = get_h_scroll();
	offset["y"] = get_v_scroll();
	Dictionary size;
	size["width"] = content->get_size().x;
	size["height"] = content->get_size().y;
	Dictionary viewport;
	viewport["width"] = usable_viewport().size.x;
	viewport["height"] = usable_viewport().size.y;
	Dictionary inset;
	for (const char *key : { "top", "left", "bottom", "right" }) {
		inset[key] = 0.0;
	}
	Dictionary event;
	event["contentOffset"] = offset;
	event["contentSize"] = size;
	event["layoutMeasurement"] = viewport;
	event["contentInset"] = inset;
	event["zoomScale"] = 1.0;
	published_context.event_sink->emit(published_context.tag, p_event, event, published_context.revision);
	if (published_context.event_sink->invalidate_geometry) {
		published_context.event_sink->invalidate_geometry();
	}
}
void RNScrollControl::_scroll_changed(double) {
	_emit_scroll("topScroll");
}
void RNScrollControl::_scroll_started() {
	dragging = true;
	set_process(true);
	if (scroll_tween.is_valid()) {
		scroll_tween->kill();
		scroll_tween.unref();
	}
	if (published_context.event_sink && published_context.event_sink->cancel_input) {
		published_context.event_sink->cancel_input();
	}
	_emit_scroll("topScrollBeginDrag");
}
void RNScrollControl::_scroll_ended() {
	if (dragging) {
		_emit_scroll("topScrollEndDrag");
		dragging = false;
	}
	if (momentum) {
		_emit_scroll("topMomentumScrollEnd");
		momentum = false;
	}
	set_process(false);
	_snap();
}
std::shared_ptr<const RNHostDescriptor> rn_scroll_descriptor(const StringName &p_name, bool p_horizontal) {
	return std::make_shared<RNScrollDescriptor>(p_name, p_horizontal);
}
std::shared_ptr<const RNHostDescriptor> rn_scroll_content_descriptor(const StringName &p_name) {
	return std::make_shared<RNScrollContentDescriptor>(p_name);
}

void RNScrollControl::_capture_anchor() {
	anchor = ObjectID();
	if (!published_props.has("maintainVisibleContentPosition") || published_props["maintainVisibleContentPosition"].get_type() != Variant::DICTIONARY || content->get_child_count() == 0) {
		return;
	}
	Control *rows = Object::cast_to<Control>(content->get_child(0));
	if (!rows) {
		return;
	}
	Dictionary config = published_props["maintainVisibleContentPosition"];
	const bool horizontal = published_props.get("horizontal", false);
	anchor_offset = Point2(get_h_scroll(), get_v_scroll());
	const int axis = horizontal ? 0 : 1;
	const int minimum = MAX(0, int(config.get("minIndexForVisible", 0)));
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
	const bool horizontal = published_props.get("horizontal", false);
	const int axis = horizontal ? 0 : 1;
	Point2 offset(get_h_scroll(), get_v_scroll());
	const double value = offset[axis];
	double target = value;
	Array offsets = published_props.get("snapToOffsets", Array());
	if (!offsets.is_empty()) {
		target = offsets[0];
		for (int i = 1; i < offsets.size(); ++i) {
			if (Math::abs(double(offsets[i]) - value) < Math::abs(target - value)) {
				target = offsets[i];
			}
		}
	} else {
		const double interval = published_props.get("snapToInterval", bool(published_props.get("pagingEnabled", false)) ? usable_viewport().size[axis] : 0.0);
		if (interval <= 0) {
			return;
		}
		const String alignment = published_props.get("snapToAlignment", "start");
		const double shift = alignment == "center" ? (usable_viewport().size[axis] - interval) / 2 : alignment == "end" ? usable_viewport().size[axis] - interval
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
	ScrollContainer::gui_input(p_event);
}
