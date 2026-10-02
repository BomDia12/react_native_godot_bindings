#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include <memory>

class RNEventTarget;
class RNHostDescriptor;

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
	String view_name;
	Dictionary props;
	Vector<Ref<RNShadowNode>> children;
	std::shared_ptr<RNEventTarget> event_target;
	std::shared_ptr<const RNHostDescriptor> descriptor;
	HashMap<String, uint64_t> declarative_prop_revisions;
	int validated_depth = 1;
	bool children_replaced = false;
	bool structurally_valid = true;
	String structural_error;

	// Native tree walks use this bound to reject hostile input before recursion.
	static constexpr int MAX_DEPTH = 1024;

	Ref<RNShadowNode> clone(bool p_new_children, const Dictionary *p_new_props) const;

	// Concatenation of the RCTRawText descendants, which is what an RCTText displays.
	String collect_text() const;

	// Iterative, so validating a hostile tree cannot overflow the stack by itself.
	static bool is_within_depth_limit(const Ref<RNShadowNode> &p_root);
};
