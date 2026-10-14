#pragma once

#include "rn_host_descriptor.h"

#include "scene/resources/font.h"

struct RNFontSnapshot {
	Ref<Font> base;
	Ref<Font> font;
	int size = 16;
	double multiplier = 1;
	Color color = Color(1, 1, 1);
	uint64_t revision = 0;
};

Ref<Font> rn_load_font_family(const String &p_family, Dictionary &r_variation);
Ref<Font> rn_font_variation(const Ref<Font> &p_base, int p_weight, bool p_italic, float p_letter_spacing);
bool rn_resolve_font(const Dictionary &p_props, const RNHostContext &p_context, RNFontSnapshot &r_font, RNError &r_error);
