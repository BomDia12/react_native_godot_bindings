#pragma once

#include "core/math/rect2.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include <memory>

class RNEventTarget;

// One node of Fabric's shadow tree.
//
// The tree is persistent and immutable: the renderer clones nodes instead of
// mutating them, and an untouched subtree must be *shared* by the clone, not
// deep-copied. RefCounted gives us that sharing; deep-copying children would
// still be correct but would quietly destroy re-render performance.
class RNShadowNode : public RefCounted {
	GDCLASS(RNShadowNode, RefCounted);

public:
	int tag = 0;
	String view_name;
	Dictionary props;
	Vector<Ref<RNShadowNode>> children;
	std::shared_ptr<RNEventTarget> event_target;

	// Layout relative to this node's Yoga parent, filled in by RNLayout::calculate().
	// Matches what Control::set_position() expects from a Godot child. Not part of the
	// JS contract.
	Rect2 layout;

	Ref<RNShadowNode> clone(bool p_new_children, const Dictionary *p_new_props) const;

	// Concatenation of the RCTRawText descendants, which is what an RCTText displays.
	String collect_text() const;
};
