#include "rn_paragraph_component_view.h"

#include "rn_text_shaper.h"

#include <react/renderer/components/text/ParagraphProps.h>
#include <react/renderer/components/text/ParagraphState.h>
#include <react/renderer/core/ConcreteState.h>

using namespace facebook::react;

RNParagraphComponentView::RNParagraphComponentView(Tag p_tag, std::shared_ptr<const RNTextShaper> p_shaper) :
		RNComponentView(p_tag, memnew(RNTextControl)), text(Object::cast_to<RNTextControl>(get_control())), shaper(std::move(p_shaper)) {
}

void RNParagraphComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &paragraph = static_cast<const ParagraphProps &>(*p_new);
	text->set_mouse_filter(paragraph.pointerEvents == PointerEventsMode::None || paragraph.pointerEvents == PointerEventsMode::BoxNone ? Control::MOUSE_FILTER_IGNORE : Control::MOUSE_FILTER_PASS);
}

void RNParagraphComponentView::update_state(const State::Shared &p_state) {
	RNComponentView::update_state(p_state);
	const auto &data = std::static_pointer_cast<const ConcreteState<ParagraphState>>(p_state)->getData();
	const auto &paragraph = static_cast<const ParagraphProps &>(*props);
	const AttributedString attributed = data.attributedString;
	const ParagraphAttributes attributes = data.paragraphAttributes;
	text->set_content(String::utf8(attributed.getString().c_str()), [shaper = shaper, attributed, attributes](float p_width, RNParagraphLayout &r_layout) { shaper->shape(attributed, attributes, p_width, r_layout); }, paragraph.isSelectable);
}

Tag RNParagraphComponentView::fragment_tag_at(const Point2 &p_point) const {
	const RNParagraphLayout &layout = text->get_layout();
	const int character = layout.hit_test(p_point);
	int offset = 0;
	for (const RNParagraphLayout::Fragment &fragment : layout.get_fragments()) {
		offset += fragment.text.length();
		if (character < offset) {
			return fragment.tag;
		}
	}
	return tag;
}
