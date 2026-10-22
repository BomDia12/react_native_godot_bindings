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

// Natural, start and end follow the layout direction, as I18nManager RTL
// applications expect. Left and right swap in right-to-left layouts too, as
// iOS's RCTResolveTextAlignment and Android's text layout do.
HorizontalAlignment horizontal_alignment(std::optional<TextAlignment> p_alignment, bool p_rtl_layout) {
	switch (p_alignment.value_or(TextAlignment::Natural)) {
		case TextAlignment::Center:
			return HORIZONTAL_ALIGNMENT_CENTER;
		case TextAlignment::Left:
			return p_rtl_layout ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT;
		case TextAlignment::Right:
			return p_rtl_layout ? HORIZONTAL_ALIGNMENT_LEFT : HORIZONTAL_ALIGNMENT_RIGHT;
		case TextAlignment::Justified:
			return HORIZONTAL_ALIGNMENT_FILL;
		case TextAlignment::End:
			return p_rtl_layout ? HORIZONTAL_ALIGNMENT_LEFT : HORIZONTAL_ALIGNMENT_RIGHT;
		default:
			return p_rtl_layout ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_LEFT;
	}
}

TextServer::OverrunBehavior overrun(EllipsizeMode p_mode) {
	return p_mode == EllipsizeMode::Clip ? TextServer::OVERRUN_TRIM_CHAR : TextServer::OVERRUN_TRIM_ELLIPSIS;
}

RNParagraphLayout::EllipsisPosition ellipsis_position(EllipsizeMode p_mode) {
	switch (p_mode) {
		case EllipsizeMode::Head:
			return RNParagraphLayout::ELLIPSIS_HEAD;
		case EllipsizeMode::Middle:
			return RNParagraphLayout::ELLIPSIS_MIDDLE;
		default:
			return RNParagraphLayout::ELLIPSIS_TAIL;
	}
}

RNParagraphLayout::DecorationStyle decoration_style(std::optional<TextDecorationStyle> p_style) {
	switch (p_style.value_or(TextDecorationStyle::Solid)) {
		case TextDecorationStyle::Double:
			return RNParagraphLayout::DECORATION_DOUBLE;
		case TextDecorationStyle::Dotted:
			return RNParagraphLayout::DECORATION_DOTTED;
		case TextDecorationStyle::Dashed:
			return RNParagraphLayout::DECORATION_DASHED;
		default:
			return RNParagraphLayout::DECORATION_SOLID;
	}
}

PackedStringArray font_features(std::optional<FontVariant> p_variant) {
	PackedStringArray features;
	const int variant = int(p_variant.value_or(FontVariant::Default));
	const std::pair<FontVariant, const char *> named[] = {
		{ FontVariant::SmallCaps, "smcp" },
		{ FontVariant::OldstyleNums, "onum" },
		{ FontVariant::LiningNums, "lnum" },
		{ FontVariant::TabularNums, "tnum" },
		{ FontVariant::ProportionalNums, "pnum" },
	};
	for (const auto &[flag, tag] : named) {
		if (variant & int(flag)) {
			features.push_back(tag);
		}
	}
	for (int set = 1; set <= 20; ++set) {
		if (variant & (int(FontVariant::StylisticOne) << (set - 1))) {
			features.push_back(vformat("ss%02d", set));
		}
	}
	return features;
}

// Godot's case mapping is one character to one character, so indices into the
// attributed string stay valid for hit testing and selection.
String transformed(const String &p_text, std::optional<TextTransform> p_transform) {
	switch (p_transform.value_or(TextTransform::None)) {
		case TextTransform::Uppercase:
			return p_text.to_upper();
		case TextTransform::Lowercase:
			return p_text.to_lower();
		case TextTransform::Capitalize: {
			String result = p_text;
			const PackedInt32Array words = TS->string_get_word_breaks(p_text);
			for (int i = 0; i + 1 < words.size(); i += 2) {
				result[words[i]] = String::chr(p_text[words[i]]).to_upper()[0];
			}
			return result;
		}
		default:
			return p_text;
	}
}

} // namespace

RNTextShaper::RNTextShaper(std::shared_ptr<const RNThemeMetricsStore> p_theme, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
		theme(std::move(p_theme)), main_thread(std::move(p_main_thread)) {
}

Ref<Font> RNTextShaper::_font(const TextAttributes &p_attributes, const RNThemeMetrics &p_metrics) const {
	// Zero keys text that asks for no weight apart from text that asks for regular.
	const int weight = p_attributes.fontWeight ? int(*p_attributes.fontWeight) : 0;
	const bool italic = p_attributes.fontStyle.value_or(FontStyle::Normal) != FontStyle::Normal;
	const float spacing = std::isnan(p_attributes.letterSpacing) ? 0 : p_attributes.letterSpacing;
	const int variant = int(p_attributes.fontVariant.value_or(FontVariant::Default));
	const FontKey key(p_attributes.fontFamily, weight, italic, spacing, variant);
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
		RNFontStyle style;
		style.weight = p_attributes.fontWeight ? weight : alias_weight;
		style.weight_requested = p_attributes.fontWeight.has_value() || variation.has("weight");
		style.italic = p_attributes.fontStyle ? italic : alias_italic;
		style.letter_spacing = spacing;
		style.features = font_features(p_attributes.fontVariant);
		font = rn_font_variation(base, style);
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

void RNTextShaper::_shape(const AttributedString &p_string, const ParagraphAttributes &p_paragraph, float p_width, float p_scale, RNParagraphLayout &r_layout) const {
	const std::shared_ptr<const RNThemeMetrics> metrics = theme->get();
	Vector<RNParagraphLayout::Fragment> fragments;
	std::optional<TextAlignment> alignment;
	std::optional<WritingDirection> direction;
	std::optional<LayoutDirection> layout_direction;
	for (const AttributedString::Fragment &fragment : p_string.getFragments()) {
		if (fragment.isAttachment()) {
			continue;
		}
		const TextAttributes &attributes = fragment.textAttributes;
		alignment = alignment ? alignment : attributes.alignment;
		direction = direction ? direction : attributes.baseWritingDirection;
		layout_direction = layout_direction ? layout_direction : attributes.layoutDirection;
		RNParagraphLayout::Fragment output;
		output.text = transformed(String::utf8(fragment.string.c_str()), attributes.textTransform);
		output.font = _font(attributes, *metrics);
		const float size = effective_font_size(attributes, metrics->text_font_size);
		output.font_size = MAX(1, int(Math::round(size * p_scale)));
		output.color = godot_color(attributes.foregroundColor, metrics->text_color);
		output.background = godot_color(attributes.backgroundColor, ::Color(0, 0, 0, 0));
		output.line_height = std::isnan(attributes.lineHeight) ? 0 : attributes.lineHeight * font_multiplier(attributes, size) * p_scale;
		const TextDecorationLineType decoration = attributes.textDecorationLineType.value_or(TextDecorationLineType::None);
		if (decoration == TextDecorationLineType::Underline || decoration == TextDecorationLineType::UnderlineStrikethrough) {
			output.decorations |= RNParagraphLayout::DECORATION_UNDERLINE;
		}
		if (decoration == TextDecorationLineType::Strikethrough || decoration == TextDecorationLineType::UnderlineStrikethrough) {
			output.decorations |= RNParagraphLayout::DECORATION_STRIKETHROUGH;
		}
		output.decoration_color = godot_color(attributes.textDecorationColor, output.color);
		output.decoration_style = decoration_style(attributes.textDecorationStyle);
		if (attributes.textShadowColor || attributes.textShadowOffset) {
			output.shadow_color = godot_color(attributes.textShadowColor, ::Color(0, 0, 0, 1.0f / 3));
			const Size offset = attributes.textShadowOffset.value_or(Size{});
			output.shadow_offset = Vector2(offset.width, offset.height) * p_scale;
			output.shadow_radius = std::isnan(attributes.textShadowRadius) ? 0 : attributes.textShadowRadius * p_scale;
		}
		output.tag = fragment.parentShadowView.tag;
		fragments.push_back(output);
	}
	const bool rtl_layout = layout_direction == LayoutDirection::RightToLeft;
	RNParagraphLayout::Attributes attributes;
	attributes.width = p_width;
	attributes.max_lines = p_paragraph.maximumNumberOfLines;
	attributes.overrun = overrun(p_paragraph.ellipsizeMode);
	attributes.ellipsis = ellipsis_position(p_paragraph.ellipsizeMode);
	attributes.alignment = horizontal_alignment(alignment, rtl_layout);
	attributes.direction = direction == WritingDirection::RightToLeft ? TextServer::DIRECTION_RTL : direction == WritingDirection::LeftToRight ? TextServer::DIRECTION_LTR
																																			   : TextServer::DIRECTION_AUTO;
	r_layout.shape(fragments, attributes);
}

void RNTextShaper::shape(const AttributedString &p_string, const ParagraphAttributes &p_paragraph, const Size2 &p_available, RNParagraphLayout &r_layout) const {
	const float width = std::isfinite(p_available.x) && p_available.x > 0 ? p_available.x : -1;
	_shape(p_string, p_paragraph, width, 1, r_layout);
	if (!p_paragraph.adjustsFontSizeToFit) {
		return;
	}
	auto fits = [&]() {
		return !r_layout.is_truncated() && (!std::isfinite(p_available.y) || r_layout.get_size().y <= p_available.y);
	};
	if (fits()) {
		return;
	}
	// Searches hundredths of the scale between minimumFontScale and 1.
	const float minimum = std::isnan(p_paragraph.minimumFontScale) ? 0.01f : CLAMP(p_paragraph.minimumFontScale, 0.01f, 1.0f);
	int low = int(Math::round(minimum * 100));
	int high = 99;
	while (low < high) {
		const int middle = (low + high + 1) / 2;
		_shape(p_string, p_paragraph, width, middle / 100.0f, r_layout);
		if (fits()) {
			low = middle;
		} else {
			high = middle - 1;
		}
	}
	_shape(p_string, p_paragraph, width, low / 100.0f, r_layout);
}
