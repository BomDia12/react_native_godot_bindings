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
	int root_tag = 0;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	uint64_t revision = 0;
	String view_name;
	Dictionary props;
	Vector<Ref<RNShadowNode>> children;
	std::shared_ptr<RNEventTarget> event_target;
	Vector<String> declarative_prop_keys;

	// Layout relative to this node's Yoga parent, filled in by RNLayout::calculate().
	// Matches what Control::set_position() expects from a Godot child. Not part of the
	// JS contract.
	Rect2 layout;

	// Layout, mounting, hit testing and measurement all walk this tree recursively, and JS
	// decides how deep it is. Trees are checked against this limit once on the way in, so
	// those walks cannot be driven into a native stack overflow. Far deeper than any real
	// React tree.
	static constexpr int MAX_DEPTH = 1024;

	Ref<RNShadowNode> clone(bool p_new_children, const Dictionary *p_new_props) const;

	// Concatenation of the RCTRawText descendants, which is what an RCTText displays.
	String collect_text() const;

	// Iterative, so validating a hostile tree cannot overflow the stack by itself.
	static bool is_within_depth_limit(const Ref<RNShadowNode> &p_root);
};
