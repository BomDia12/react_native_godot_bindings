#include "rn_view_component_view.h"

#include "../interop/rn_dynamic.h"

#include <react/renderer/components/view/ViewEventEmitter.h>

RNViewComponentView::RNViewComponentView(facebook::react::Tag p_tag) :
		RNComponentView(p_tag, memnew(RNViewControl)), view(Object::cast_to<RNViewControl>(get_control())) {
}

RNStyleBox::Paint rn_view_paint(const facebook::react::ViewProps &p_props, const facebook::react::LayoutMetrics &p_layout) {
	using namespace facebook::react;
	const BorderMetrics border = p_props.resolveBorderMetrics(p_layout);
	RNStyleBox::Paint paint;
	paint.border_box = rn_rounded_rect(Rect2(Point2(), Size2(p_layout.frame.size.width, p_layout.frame.size.height)), border.borderRadii);
	paint.widths[SIDE_LEFT] = border.borderWidths.left;
	paint.widths[SIDE_TOP] = border.borderWidths.top;
	paint.widths[SIDE_RIGHT] = border.borderWidths.right;
	paint.widths[SIDE_BOTTOM] = border.borderWidths.bottom;
	paint.colors[SIDE_LEFT] = rn_godot_color(border.borderColors.left, ::Color(0, 0, 0));
	paint.colors[SIDE_TOP] = rn_godot_color(border.borderColors.top, ::Color(0, 0, 0));
	paint.colors[SIDE_RIGHT] = rn_godot_color(border.borderColors.right, ::Color(0, 0, 0));
	paint.colors[SIDE_BOTTOM] = rn_godot_color(border.borderColors.bottom, ::Color(0, 0, 0));
	paint.styles[SIDE_LEFT] = border.borderStyles.left;
	paint.styles[SIDE_TOP] = border.borderStyles.top;
	paint.styles[SIDE_RIGHT] = border.borderStyles.right;
	paint.styles[SIDE_BOTTOM] = border.borderStyles.bottom;
	paint.background = rn_godot_color(p_props.backgroundColor);
	paint.images = p_props.backgroundImage;
	paint.sizes = p_props.backgroundSize;
	paint.positions = p_props.backgroundPosition;
	paint.repeats = p_props.backgroundRepeat;
	paint.shadows = p_props.boxShadow;
	paint.outline_color = rn_godot_color(p_props.outlineColor, ::Color(0, 0, 0));
	paint.outline_width = p_props.outlineWidth;
	paint.outline_offset = p_props.outlineOffset;
	paint.outline_style = p_props.outlineStyle;
	return paint;
}

Control *RNViewComponentView::get_child_container() {
	if (clip) {
		return clip;
	}
	return effect ? effect->get_painter() : view;
}

void RNViewComponentView::_restructure(bool p_effect, bool p_clip) {
	if (p_effect == (effect != nullptr) && p_clip == (clip != nullptr)) {
		return;
	}
	Control *previous = get_child_container();
	Vector<Control *> children;
	Control *focused = nullptr;
	for (int i = 0; i < previous->get_child_count(); ++i) {
		if (Control *child = Object::cast_to<Control>(previous->get_child(i))) {
			children.push_back(child);
			if (Control *owner = child->get_viewport()->gui_get_focus_owner(); owner && (owner == child || child->is_ancestor_of(owner))) {
				focused = owner;
			}
		}
	}
	for (Control *child : children) {
		previous->remove_child(child);
	}
	if (clip) {
		clip->get_parent()->remove_child(clip);
		memdelete(clip);
		clip = nullptr;
	}
	if (p_effect != (effect != nullptr)) {
		if (effect) {
			view->remove_child(effect);
			memdelete(effect);
			effect = nullptr;
		} else {
			effect = memnew(RNEffectHost);
			view->add_child(effect, false, Node::INTERNAL_MODE_FRONT);
		}
	}
	if (p_clip) {
		clip = memnew(RNClipControl);
		Control *parent = effect ? effect->get_painter() : view;
		parent->add_child(clip, false, Node::INTERNAL_MODE_FRONT);
	}
	Control *container = get_child_container();
	for (Control *child : children) {
		container->add_child(child);
		rn_inherit_clip(child);
	}
	if (focused && focused->is_inside_tree()) {
		focused->grab_focus();
	}
}

void RNViewComponentView::_apply_style() {
	using namespace facebook::react;
	const RNStyleBox::Paint paint = rn_view_paint(*props, layout_metrics);
	const ::Vector<RNFilterStage> stages = rn_filter_stages(props->filter);
	const bool needs_effect = !stages.is_empty() || props->mixBlendMode != BlendMode::Normal || props->isolation == Isolation::Isolate;
	const bool needs_clip = props->getClipsContentToBounds();
	_restructure(needs_effect, needs_clip);
	view->set_clip_contents(false);
	view->set_paint(paint, !effect);
	const Size2 size = paint.border_box.rect.size;
	if (effect) {
		effect->configure(size, MAX(paint.extent(), rn_filter_extent(stages)), stages, props->mixBlendMode);
		effect->get_painter()->set_paint(paint);
	}
	if (clip) {
		clip->set_size(size);
		RNRoundedRect shape = paint.padding_box();
		clip->set_shape(shape);
	}
}

void RNViewComponentView::update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	using facebook::react::PointerEventsMode;
	const bool self_targetable = props->pointerEvents != PointerEventsMode::None && props->pointerEvents != PointerEventsMode::BoxNone;
	view->set_focus_mode(self_targetable && props->focusable ? Control::FOCUS_ALL : Control::FOCUS_NONE);
	view->set_menu_entries(self_targetable ? Array(rn_variant_from_dynamic(props->godotContextMenu)) : Array());
	Dictionary accessibility;
	accessibility["accessible"] = props->accessible;
	accessibility["accessibilityLabel"] = String::utf8(props->accessibilityLabel.c_str());
	accessibility["accessibilityHint"] = String::utf8(props->accessibilityHint.c_str());
	accessibility["accessibilityElementsHidden"] = props->accessibilityElementsHidden;
	view->set_meta("react_native_accessibility", accessibility);
	_apply_style();
}

void RNViewComponentView::update_layout(const facebook::react::LayoutMetrics &p_metrics) {
	RNComponentView::update_layout(p_metrics);
	if (props) {
		_apply_style();
	}
}

void RNViewComponentView::update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const facebook::react::ViewEventEmitter>(p_emitter);
	if (!emitter) {
		view->set_menu_callbacks(nullptr, nullptr);
		view->set_focus_callback(nullptr);
		return;
	}
	view->set_menu_callbacks([emitter](const String &p_id) { emitter->onGodotContextMenuAction(p_id.utf8().get_data()); }, nullptr);
	view->set_focus_callback([emitter](bool p_focused) {
		if (p_focused) {
			emitter->onFocus();
		} else {
			emitter->onBlur();
		}
	});
}

void RNViewComponentView::handle_command(const std::string &p_name, const folly::dynamic &) {
	if (p_name == "focus") {
		view->grab_focus();
	} else if (p_name == "blur") {
		view->release_focus();
	}
}
