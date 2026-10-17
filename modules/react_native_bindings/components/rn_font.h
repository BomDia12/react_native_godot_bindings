#pragma once

#include "scene/resources/font.h"

Ref<Font> rn_load_font_family(const String &p_family, Dictionary &r_variation);
Ref<Font> rn_font_variation(const Ref<Font> &p_base, int p_weight, bool p_italic, float p_letter_spacing);
