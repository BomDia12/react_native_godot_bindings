#pragma once

#include "rn_host_descriptor.h"

#include "scene/resources/font.h"

struct RNFontSnapshot {
	Ref<Font> base;
	Ref<Font> font;
	int size = 16;
	Color color = Color(1, 1, 1);
	uint64_t revision = 0;
};

bool rn_resolve_font(const Dictionary &p_props, const RNHostContext &p_context, RNFontSnapshot &r_font, RNError &r_error);
