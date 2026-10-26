#include "rn_paragraph_component_view.h"

#include "rn_text_shaper.h"
#include "rn_view_component_view.h"

#include <react/renderer/components/text/ParagraphProps.h>
#include <react/renderer/components/text/ParagraphState.h>
#include <react/renderer/components/view/accessibilityPropsConversions.h>
#include <react/renderer/core/ConcreteState.h>

using namespace facebook::react;

RNParagraphComponentView::RNParagraphComponentView(Tag p_tag, std::shared_ptr<const RNTextShaper> p_shaper) :
		RNComponentView(p_tag, memnew(RNTextControl)), text(Object::cast_to<RNTextControl>(get_control())), shaper(std::move(p_shaper)) {
	box.instantiate();
	text->set_box(box);
}

void RNParagraphComponentView::_apply_box() {
	if (props) {
		box->set_paint(rn_view_paint(*props, layout_metrics));
		text->queue_redraw();
	}
}

void RNParagraphComponentView::update_layout(const LayoutMetrics &p_metrics) {
	const EdgeInsets &insets = p_metrics.contentInsets;
	text->set_content_insets(insets.left, insets.top, insets.right, insets.bottom);
	RNComponentView::update_layout(p_metrics);
	_apply_box();
}

void RNParagraphComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &paragraph = static_cast<const ParagraphProps &>(*p_new);
	text->set_mouse_filter(paragraph.pointerEvents == PointerEventsMode::None || paragraph.pointerEvents == PointerEventsMode::BoxNone ? Control::MOUSE_FILTER_IGNORE : Control::MOUSE_FILTER_PASS);
	_apply_box();
	const std::string role = paragraph.role != Role::None ? toString(paragraph.role) : paragraph.accessibilityRole;
	if (role == "link") {
		text->set_accessibility_role(AccessibilityServerEnums::ROLE_LINK);
	} else if (role == "header" || role == "heading") {
		text->set_accessibility_role(AccessibilityServerEnums::ROLE_STATIC_TEXT, "heading");
	} else {
		text->set_accessibility_role(AccessibilityServerEnums::ROLE_STATIC_TEXT);
	}
}

void RNParagraphComponentView::update_state(const State::Shared &p_state) {
	RNComponentView::update_state(p_state);
	const auto &data = std::static_pointer_cast<const ConcreteState<ParagraphState>>(p_state)->getData();
	const auto &paragraph = static_cast<const ParagraphProps &>(*props);
	attributed = data.attributedString;
	const ParagraphAttributes attributes = data.paragraphAttributes;
	text->set_content(String::utf8(attributed.getString().c_str()), [shaper = shaper, attributed = attributed, attributes](const Size2 &p_size, RNParagraphLayout &r_layout) { shaper->shape(attributed, attributes, p_size, r_layout); }, paragraph.isSelectable);
}

RNTouchTarget RNParagraphComponentView::touch_target_at(const Point2 &p_local) const {
	const int character = text->get_layout().character_at(text->to_content(p_local));
	if (character < 0) {
		return RNComponentView::touch_target_at(p_local);
	}
	int offset = 0;
	for (const AttributedString::Fragment &fragment : attributed.getFragments()) {
		if (fragment.isAttachment()) {
			continue;
		}
		offset += String::utf8(fragment.string.c_str()).length();
		if (character < offset) {
			auto emitter = std::dynamic_pointer_cast<const TouchEventEmitter>(fragment.parentShadowView.eventEmitter);
			return emitter ? RNTouchTarget{ fragment.parentShadowView.tag, emitter } : RNComponentView::touch_target_at(p_local);
		}
	}
	return RNComponentView::touch_target_at(p_local);
}
