#pragma once

#include "rn_shadow_node.h"

#include "core/math/vector2.h"

namespace RNLayout {

    // Builds a Yoga tree from the shadow tree, runs flexbox against p_available, and
    // writes absolute rects back into each RNShadowNode::layout. The Yoga tree is
    // rebuilt per commit and freed here: shadow subtrees are shared between clones,
    // and a YGNode can only have one owner, so a per-node Yoga handle cannot be reused
    // safely across trees.
    void calculate(const Ref<RNShadowNode> &p_root, const Size2 &p_available);

} //namespace RNLayout
