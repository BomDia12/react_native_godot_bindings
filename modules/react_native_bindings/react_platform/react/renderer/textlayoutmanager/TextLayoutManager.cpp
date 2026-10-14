#include "TextLayoutManager.h"

#include "components/rn_text_shaper.h"

#include <cmath>

namespace facebook::react {

namespace {

float layout_width(const LayoutConstraints &constraints) {
	return std::isfinite(constraints.maximumSize.width) ? constraints.maximumSize.width : -1;
}

} // namespace

TextLayoutManager::TextLayoutManager(const std::shared_ptr<const ContextContainer> &contextContainer) :
		contextContainer_(contextContainer),
		textMeasureCache_(kSimpleThreadSafeCacheSizeCap),
		shaper_(contextContainer->at<std::shared_ptr<const RNTextShaper>>(RNTextShaper::CONTEXT_KEY)) {
}

TextMeasurement TextLayoutManager::measure(const AttributedStringBox &attributedStringBox, const ParagraphAttributes &paragraphAttributes, const TextLayoutContext &, const LayoutConstraints &layoutConstraints) const {
	const AttributedString &attributedString = attributedStringBox.getValue();
	return textMeasureCache_.get({ attributedString, paragraphAttributes, layoutConstraints }, [&]() {
		RNParagraphLayout layout;
		shaper_->shape(attributedString, paragraphAttributes, layout_width(layoutConstraints), layout);
		TextMeasurement::Attachments attachments;
		for (const auto &fragment : attributedString.getFragments()) {
			if (fragment.isAttachment()) {
				attachments.push_back(TextMeasurement::Attachment{ .frame = {}, .isClipped = true });
			}
		}
		const Size2 size = layout.get_size().ceil();
		return TextMeasurement{ .size = layoutConstraints.clamp({ .width = size.x, .height = size.y }), .attachments = attachments };
	});
}

LinesMeasurements TextLayoutManager::measureLines(const AttributedStringBox &attributedStringBox, const ParagraphAttributes &paragraphAttributes, const Size &size) const {
	RNParagraphLayout layout;
	shaper_->shape(attributedStringBox.getValue(), paragraphAttributes, size.width, layout);
	const String text = String::utf8(attributedStringBox.getValue().getString().c_str());
	LinesMeasurements lines;
	for (const RNParagraphLayout::Line &line : layout.get_lines()) {
		const Rect2 bounds = line.bounds;
		const Float ascender = line.baseline - bounds.position.y;
		const Ref<Font> font = layout.get_fragments().is_empty() ? Ref<Font>() : layout.get_fragments()[0].font;
		const int font_size = layout.get_fragments().is_empty() ? 16 : layout.get_fragments()[0].font_size;
		const Float x_height = font.is_valid() ? font->get_char_size('x', font_size).y : 0;
		const Float cap_height = font.is_valid() ? font->get_char_size('H', font_size).y : 0;
		lines.emplace_back(
				text.substr(line.range.x, line.range.y - line.range.x).utf8().get_data(),
				Rect{ .origin = { .x = bounds.position.x, .y = bounds.position.y }, .size = { .width = bounds.size.x, .height = bounds.size.y } },
				bounds.size.y - ascender, cap_height, ascender, x_height);
	}
	return lines;
}

} // namespace facebook::react
