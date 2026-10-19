#include "rn_component_view.h"

#include "../components/rn_clip.h"
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

namespace {

constexpr const char *CURSOR_META = "_react_native_cursor";

// Cursors with no Godot shape map to the arrow.
std::optional<Control::CursorShape> cursor_shape(Cursor p_cursor) {
	switch (p_cursor) {
		case Cursor::Default:
			return Control::CURSOR_ARROW;
		case Cursor::Pointer:
			return Control::CURSOR_POINTING_HAND;
		case Cursor::Text:
			return Control::CURSOR_IBEAM;
		case Cursor::Wait:
			return Control::CURSOR_WAIT;
		case Cursor::Progress:
			return Control::CURSOR_BUSY;
		case Cursor::Move:
		case Cursor::AllScroll:
			return Control::CURSOR_MOVE;
		case Cursor::NotAllowed:
		case Cursor::NoDrop:
			return Control::CURSOR_FORBIDDEN;
		case Cursor::Help:
			return Control::CURSOR_HELP;
		case Cursor::Crosshair:
			return Control::CURSOR_CROSS;
		case Cursor::Grab:
		case Cursor::Grabbing:
			return Control::CURSOR_DRAG;
		case Cursor::NSResize:
		case Cursor::NResize:
		case Cursor::SResize:
			return Control::CURSOR_VSIZE;
		case Cursor::EWResize:
		case Cursor::EResize:
		case Cursor::WResize:
			return Control::CURSOR_HSIZE;
		case Cursor::NESWResize:
		case Cursor::NEResize:
		case Cursor::SWResize:
			return Control::CURSOR_BDIAGSIZE;
		case Cursor::NWSEResize:
		case Cursor::NWResize:
		case Cursor::SEResize:
			return Control::CURSOR_FDIAGSIZE;
		case Cursor::ColResize:
			return Control::CURSOR_HSPLIT;
		case Cursor::RowResize:
			return Control::CURSOR_VSPLIT;
		default:
			return std::nullopt;
	}
}

void refresh_cursors(Node *p_node, Control::CursorShape p_inherited) {
	Control::CursorShape shape = p_inherited;
	if (Control *control = Object::cast_to<Control>(p_node); control && control->has_meta(CURSOR_META)) {
		const int own = control->get_meta(CURSOR_META);
		shape = own < 0 ? p_inherited : Control::CursorShape(own);
		control->set_default_cursor_shape(shape);
	}
	for (int i = 0; i < p_node->get_child_count(true); ++i) {
		refresh_cursors(p_node->get_child(i, true), shape);
	}
}

} // namespace

RNRoundedRect rn_rounded_rect(const Rect2 &p_rect, const BorderRadii &p_radii) {
	const Vector2 radii[4] = {
		Vector2(p_radii.topLeft.horizontal, p_radii.topLeft.vertical),
		Vector2(p_radii.topRight.horizontal, p_radii.topRight.vertical),
		Vector2(p_radii.bottomRight.horizontal, p_radii.bottomRight.vertical),
		Vector2(p_radii.bottomLeft.horizontal, p_radii.bottomLeft.vertical),
	};
	return RNRoundedRect::normalized(p_rect, radii);
}

void rn_refresh_cursors(Control *p_subtree) {
	Control::CursorShape inherited = Control::CURSOR_ARROW;
	for (Node *node = p_subtree->get_parent(); node; node = node->get_parent()) {
		if (node->has_meta(CURSOR_META) && int(node->get_meta(CURSOR_META)) >= 0) {
			inherited = Control::CursorShape(int(node->get_meta(CURSOR_META)));
			break;
		}
	}
	refresh_cursors(p_subtree, inherited);
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
	const std::optional<Control::CursorShape> cursor = p_props.cursor == Cursor::Auto ? std::nullopt : cursor_shape(p_props.cursor);
	if (p_props.cursor != Cursor::Auto && !cursor && !control->has_meta("_react_native_cursor_reported")) {
		control->set_meta("_react_native_cursor_reported", true);
		WARN_PRINT(vformat("E_UNSUPPORTED_STYLE: React tag %d uses a cursor with no Godot shape; the arrow is shown.", tag));
	}
	const int cursor_value = p_props.cursor == Cursor::Auto ? -1 : int(cursor.value_or(Control::CURSOR_ARROW));
	if (!control->has_meta(CURSOR_META) || int(control->get_meta(CURSOR_META)) != cursor_value) {
		control->set_meta(CURSOR_META, cursor_value);
		rn_refresh_cursors(control);
	}
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
	// Upstream resolves transforms around the view's center, where iOS and Android anchor layers.
	control->set_offset_transform_pivot_ratio(Vector2(0.5, 0.5));
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

// Yoga frames are already mirrored for right-to-left layouts, and Godot would
// mirror an RTL control's position again, so component controls stay
// left-to-right. Views that hold direction-sensitive Godot controls, such as
// text editors, pass the direction to them.
void RNComponentView::update_layout(const LayoutMetrics &p_metrics) {
	layout_metrics = p_metrics;
	control->set_visible(p_metrics.displayType != DisplayType::None);
	control->set_layout_direction(Control::LAYOUT_DIRECTION_LTR);
	control->set_position(Point2(p_metrics.frame.origin.x, p_metrics.frame.origin.y));
	control->set_size(Size2(p_metrics.frame.size.width, p_metrics.frame.size.height));
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
	rn_inherit_clip(child);
	rn_refresh_cursors(child);
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
