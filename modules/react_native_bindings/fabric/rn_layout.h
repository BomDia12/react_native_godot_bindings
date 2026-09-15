#pragma once

#include "rn_shadow_node.h"

#include <yoga/Yoga.h>

namespace RNLayout {

void reset_style(YGNodeRef p_node);
void apply_style(YGNodeRef p_node, const Dictionary &p_style);
float text_font_size(const Dictionary &p_props);

} //namespace RNLayout
