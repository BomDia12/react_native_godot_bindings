#pragma once

#include "scene/resources/font.h"

Ref<Font> rn_load_font_family(const String &p_family, Dictionary &r_variation);
struct RNFontStyle {
	int weight = 400;
	bool italic = false;
	float letter_spacing = 0;
	// OpenType feature tags, such as "smcp" or "tnum", turned on.
	PackedStringArray features;

	bool is_default() const { return weight == 400 && !italic && letter_spacing == 0 && features.is_empty(); }
};

// p_base with p_style applied, including to every font in its fallback chain,
// so fallback glyphs match the requested weight, style and features.
Ref<Font> rn_font_variation(const Ref<Font> &p_base, const RNFontStyle &p_style);
