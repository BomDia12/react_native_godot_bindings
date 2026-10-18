#include "rn_component_view.h"

#include "../interop/rn_dynamic.h"

#include <react/renderer/graphics/Color.h>

using namespace facebook::react;

namespace {

bool is_component_control(Node *p_node) {
	return p_node->has_meta(RNComponentView::TAG_META) && !p_node->has_meta(RNComponentView::DETACHING_META);
}

} // namespace

::Color rn_godot_color(const SharedColor &p_color, const ::Color &p_default) {
	if (!p_color) {
		return p_default;
	}
	const ColorComponents components = colorComponentsFromColor(p_color);
	return ::Color(components.red, components.green, components.blue, components.alpha);
}

Ref<StyleBoxFlat> rn_view_stylebox(const ViewProps &p_props, const LayoutMetrics &p_layout) {
	Ref<StyleBoxFlat> box;
	box.instantiate();
	box->set_bg_color(rn_godot_color(p_props.backgroundColor));
	const BorderMetrics border = p_props.resolveBorderMetrics(p_layout);
	box->set_border_color(rn_godot_color(border.borderColors.top ? border.borderColors.top : border.borderColors.left));
	box->set_border_width(SIDE_LEFT, int(border.borderWidths.left));
	box->set_border_width(SIDE_TOP, int(border.borderWidths.top));
	box->set_border_width(SIDE_RIGHT, int(border.borderWidths.right));
	box->set_border_width(SIDE_BOTTOM, int(border.borderWidths.bottom));
	box->set_corner_radius(CORNER_TOP_LEFT, int(border.borderRadii.topLeft.horizontal));
	box->set_corner_radius(CORNER_TOP_RIGHT, int(border.borderRadii.topRight.horizontal));
	box->set_corner_radius(CORNER_BOTTOM_LEFT, int(border.borderRadii.bottomLeft.horizontal));
	box->set_corner_radius(CORNER_BOTTOM_RIGHT, int(border.borderRadii.bottomRight.horizontal));
	for (const BoxShadow &shadow : p_props.boxShadow) {
		if (!shadow.inset) {
			box->set_shadow_color(rn_godot_color(shadow.color, ::Color(0, 0, 0, 1)));
			box->set_shadow_size(MAX(0, int(shadow.blurRadius + shadow.spreadDistance)));
			box->set_shadow_offset(Point2(shadow.offsetX, shadow.offsetY));
			break;
		}
	}
	return box;
}

RNComponentView::RNComponentView(Tag p_tag, Control *p_control) :
		control(p_control), control_id(p_control->get_instance_id()), tag(p_tag) {
	control->set_meta(TAG_META, tag);
}

RNComponentView::~RNComponentView() {
	if (!owns_control) {
		if (ObjectDB::get_instance(control_id)) {
			control->remove_meta(TAG_META);
		}
		return;
	}
	if (ObjectDB::get_instance(control_id)) {
		if (control->get_parent()) {
			control->get_parent()->remove_child(control);
		}
		memdelete(control);
	}
}

void RNComponentView::update_props(const Props::Shared &, const Props::Shared &p_new) {
	props = std::static_pointer_cast<const ViewProps>(p_new);
	apply_view_props(*props);
}

// Native Animated changes props between commits. They layer over the props
// this view applied last, as the next commit would.
void RNComponentView::synchronously_update_props(const ComponentDescriptor &p_descriptor, const PropsParserContext &p_context, const folly::dynamic &p_props) {
	const Props::Shared old_props = props;
	update_props(old_props, p_descriptor.cloneProps(p_context, old_props, RawProps(p_props)));
}

void RNComponentView::apply_view_props(const ViewProps &p_props) {
	control->set_modulate(::Color(1, 1, 1, p_props.opacity));
	control->set_clip_contents(p_props.getClipsContentToBounds());
	const bool branch_enabled = p_props.pointerEvents != PointerEventsMode::None;
	control->set_mouse_filter(branch_enabled ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
	control->set_mouse_behavior_recursive(branch_enabled ? Control::MOUSE_BEHAVIOR_INHERITED : Control::MOUSE_BEHAVIOR_DISABLED);
	control->set_meta("react_native_test_id", String::utf8(p_props.testId.c_str()));
	apply_transform();
}

void RNComponentView::apply_transform() {
	if (!props) {
		return;
	}
	const Transform transform = props->resolveTransform(layout_metrics);
	const Transform2D matrix(transform.matrix[0], transform.matrix[1], transform.matrix[4], transform.matrix[5], transform.matrix[12], transform.matrix[13]);
	control->set_offset_transform_enabled(matrix != Transform2D());
	control->set_offset_transform_visual_only(false);
	control->set_offset_transform_pivot_ratio(Vector2());
	control->set_offset_transform_pivot(Vector2());
	control->set_offset_transform_position(matrix.get_origin());
	control->set_offset_transform_rotation(matrix.get_rotation());
	control->set_offset_transform_scale(matrix.get_scale());
	if (!Math::is_zero_approx(matrix.get_skew()) && !control->has_meta("_react_native_skew_reported")) {
		control->set_meta("_react_native_skew_reported", true);
		WARN_PRINT(vformat("E_UNSUPPORTED_TRANSFORM: React tag %d uses skew, which Godot controls do not support; only translation, rotation and scale apply.", tag));
	}
}

void RNComponentView::update_state(const State::Shared &p_state) {
	state = p_state;
}

void RNComponentView::update_layout(const LayoutMetrics &p_metrics) {
	layout_metrics = p_metrics;
	control->set_visible(p_metrics.displayType != DisplayType::None);
	control->set_position(Point2(p_metrics.frame.origin.x, p_metrics.frame.origin.y));
	control->set_size(Size2(p_metrics.frame.size.width, p_metrics.frame.size.height));
	control->set_layout_direction(p_metrics.layoutDirection == LayoutDirection::RightToLeft ? Control::LAYOUT_DIRECTION_RTL : Control::LAYOUT_DIRECTION_LTR);
	apply_transform();
}

void RNComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	event_emitter = p_emitter;
}

void RNComponentView::insert_child(RNComponentView *p_child, int p_index) {
	Control *container = get_child_container();
	Control *child = p_child->get_control();
	child->remove_meta(DETACHING_META);
	Node *anchor = nullptr;
	int react_index = 0;
	for (int i = 0; i < container->get_child_count(); ++i) {
		Node *sibling = container->get_child(i);
		if (sibling != child && is_component_control(sibling) && react_index++ == p_index) {
			anchor = sibling;
			break;
		}
	}
	if (child->get_parent() != container) {
		if (child->get_parent()) {
			child->get_parent()->remove_child(child);
		}
		container->add_child(child);
	}
	if (!anchor) {
		container->move_child(child, container->get_child_count() - 1);
	} else {
		container->move_child(child, anchor->get_index() - (child->get_index() < anchor->get_index() ? 1 : 0));
	}
}

// Fabric moves a view as Remove then Insert. Detaching right away would drop
// Godot focus and state, so the control stays until the transaction ends.
void RNComponentView::remove_child(RNComponentView *p_child) {
	Control *child = p_child->get_control();
	if (child->get_parent() == get_child_container()) {
		child->set_meta(DETACHING_META, true);
	}
}

void RNComponentView::finish_removal() {
	if (control->has_meta(DETACHING_META)) {
		control->remove_meta(DETACHING_META);
		if (control->get_parent()) {
			control->get_parent()->remove_child(control);
		}
	}
}

RNTouchTarget RNComponentView::touch_target_at(const Point2 &) const {
	return { tag, std::dynamic_pointer_cast<const TouchEventEmitter>(event_emitter) };
}
