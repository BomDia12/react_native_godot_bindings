#include "rn_text_shaper.h"

#include "../host/rn_main_thread_queue.h"
#include "rn_font.h"

#include <react/renderer/graphics/Color.h>

#include <cmath>

namespace {

using namespace facebook::react;

float effective_font_size(const TextAttributes &p_attributes, float p_default_size) {
	const float size = std::isnan(p_attributes.fontSize) ? p_default_size : p_attributes.fontSize;
	if (!p_attributes.allowFontScaling.value_or(true) || std::isnan(p_attributes.fontSizeMultiplier)) {
		return size;
	}
	float multiplier = p_attributes.fontSizeMultiplier;
	if (!std::isnan(p_attributes.maxFontSizeMultiplier) && p_attributes.maxFontSizeMultiplier >= 1) {
		multiplier = std::min(multiplier, p_attributes.maxFontSizeMultiplier);
	}
	return size * multiplier;
}

float font_multiplier(const TextAttributes &p_attributes, float p_size) {
	return std::isnan(p_attributes.fontSize) || p_attributes.fontSize == 0 ? 1 : p_size / p_attributes.fontSize;
}

::Color godot_color(const SharedColor &p_color, const ::Color &p_default) {
	if (!p_color) {
		return p_default;
	}
	const ColorComponents components = colorComponentsFromColor(p_color);
	return ::Color(components.red, components.green, components.blue, components.alpha);
}

HorizontalAlignment horizontal_alignment(std::optional<TextAlignment> p_alignment, bool p_rtl) {
	switch (p_alignment.value_or(TextAlignment::Natural)) {
		case TextAlignment::Center:
			return HORIZONTAL_ALIGNMENT_CENTER;
		case TextAlignment::Right:
			return HORIZONTAL_ALIGNMENT_RIGHT;
		case TextAlignment::Justified:
			return HORIZONTAL_ALIGNMENT_FILL;
		case TextAlignment::End:
			return p_rtl ? HORIZONTAL_ALIGNMENT_LEFT : HORIZONTAL_ALIGNMENT_RIGHT;
		case TextAlignment::Start:
			return p_rtl ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT;
		default:
			return HORIZONTAL_ALIGNMENT_LEFT;
	}
}

TextServer::OverrunBehavior overrun(EllipsizeMode p_mode) {
	return p_mode == EllipsizeMode::Clip ? TextServer::OVERRUN_TRIM_CHAR : TextServer::OVERRUN_TRIM_ELLIPSIS;
}

} // namespace

RNTextShaper::RNTextShaper(std::shared_ptr<const RNThemeMetricsStore> p_theme, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
		theme(std::move(p_theme)), main_thread(std::move(p_main_thread)) {
}

Ref<Font> RNTextShaper::_font(const TextAttributes &p_attributes, const RNThemeMetrics &p_metrics) const {
	const int weight = p_attributes.fontWeight ? int(*p_attributes.fontWeight) : 400;
	const bool italic = p_attributes.fontStyle.value_or(FontStyle::Normal) != FontStyle::Normal;
	const float spacing = std::isnan(p_attributes.letterSpacing) ? 0 : p_attributes.letterSpacing;
	const FontKey key(p_attributes.fontFamily, weight, italic, spacing);
	{
		std::lock_guard lock(mutex);
		auto cached = fonts.find(key);
		if (cached != fonts.end()) {
			return cached->second;
		}
	}
	Ref<Font> font;
	main_thread->run_sync([&]() {
		Ref<Font> base = p_metrics.text_font;
		Dictionary variation;
		if (!p_attributes.fontFamily.empty()) {
			base = rn_load_font_family(String::utf8(p_attributes.fontFamily.c_str()), variation);
		}
		if (base.is_null()) {
			ERR_PRINT(vformat("fontFamily '%s' did not resolve to a registered alias or a res:// Godot Font.", String::utf8(p_attributes.fontFamily.c_str())));
			base = p_metrics.text_font;
		}
		const String variation_weight = variation.get("weight", "normal");
		const int alias_weight = variation_weight == "bold" ? 700 : variation_weight.is_valid_int() ? variation_weight.to_int()
																									: 400;
		const bool alias_italic = String(variation.get("style", "normal")) == "italic";
		font = rn_font_variation(base, p_attributes.fontWeight ? weight : alias_weight, p_attributes.fontStyle ? italic : alias_italic, spacing);
	});
	if (font.is_null()) {
		return p_metrics.text_font;
	}
	std::lock_guard lock(mutex);
	fonts[key] = font;
	return font;
}

Ref<Font> RNTextShaper::resolve_font(const TextAttributes &p_attributes, int &r_size, ::Color &r_color) const {
	const std::shared_ptr<const RNThemeMetrics> metrics = theme->get();
	r_size = MAX(1, int(Math::round(effective_font_size(p_attributes, metrics->text_font_size))));
	r_color = godot_color(p_attributes.foregroundColor, metrics->text_color);
	return _font(p_attributes, *metrics);
}

void RNTextShaper::shape(const AttributedString &p_string, const ParagraphAttributes &p_paragraph, float p_width, RNParagraphLayout &r_layout) const {
	const std::shared_ptr<const RNThemeMetrics> metrics = theme->get();
	Vector<RNParagraphLayout::Fragment> fragments;
	std::optional<TextAlignment> alignment;
	std::optional<WritingDirection> direction;
	for (const AttributedString::Fragment &fragment : p_string.getFragments()) {
		if (fragment.isAttachment()) {
			continue;
		}
		const TextAttributes &attributes = fragment.textAttributes;
		alignment = alignment ? alignment : attributes.alignment;
		direction = direction ? direction : attributes.baseWritingDirection;
		RNParagraphLayout::Fragment output;
		output.text = String::utf8(fragment.string.c_str());
		output.font = _font(attributes, *metrics);
		const float size = effective_font_size(attributes, metrics->text_font_size);
		output.font_size = MAX(1, int(Math::round(size)));
		output.color = godot_color(attributes.foregroundColor, metrics->text_color);
		output.background = godot_color(attributes.backgroundColor, ::Color(0, 0, 0, 0));
		output.line_height = std::isnan(attributes.lineHeight) ? 0 : attributes.lineHeight * font_multiplier(attributes, size);
		const TextDecorationLineType decoration = attributes.textDecorationLineType.value_or(TextDecorationLineType::None);
		if (decoration == TextDecorationLineType::Underline || decoration == TextDecorationLineType::UnderlineStrikethrough) {
			output.decorations |= RNParagraphLayout::DECORATION_UNDERLINE;
		}
		if (decoration == TextDecorationLineType::Strikethrough || decoration == TextDecorationLineType::UnderlineStrikethrough) {
			output.decorations |= RNParagraphLayout::DECORATION_STRIKETHROUGH;
		}
		output.tag = fragment.parentShadowView.tag;
		fragments.push_back(output);
	}
	const bool rtl = direction == WritingDirection::RightToLeft;
	RNParagraphLayout::Attributes attributes;
	attributes.width = p_width;
	attributes.max_lines = p_paragraph.maximumNumberOfLines;
	attributes.overrun = overrun(p_paragraph.ellipsizeMode);
	attributes.alignment = horizontal_alignment(alignment, rtl);
	attributes.direction = rtl ? TextServer::DIRECTION_RTL : direction == WritingDirection::LeftToRight ? TextServer::DIRECTION_LTR
																										: TextServer::DIRECTION_AUTO;
	r_layout.shape(fragments, attributes);
}
