#include "rn_shadow_node.h"

#include "core/templates/local_vector.h"

Ref<RNShadowNode> RNShadowNode::clone(bool p_new_children, const Dictionary *p_new_props) const {
	Ref<RNShadowNode> copy;
	copy.instantiate();

	copy->tag = tag;
	copy->root_tag = root_tag;
	copy->runtime_generation = runtime_generation;
	copy->surface_epoch = surface_epoch;
	copy->view_name = view_name;
	copy->props = p_new_props ? p_new_props->duplicate(true) : props.duplicate(true);
	copy->event_target = event_target;
	copy->declarative_prop_revisions = declarative_prop_revisions;
	copy->validated_depth = p_new_children ? 1 : validated_depth;
	copy->children_replaced = p_new_children;
	copy->structurally_valid = structurally_valid;
	copy->structural_error = structural_error;

	if (!p_new_children) {
		// Shares the child subtrees by reference; the renderer replaces only what changed.
		copy->children = children;
	}

	return copy;
}

bool RNShadowNode::is_within_depth_limit(const Ref<RNShadowNode> &p_root) {
	struct PendingNode {
		const RNShadowNode *node;
		int depth;
	};

	if (p_root.is_null()) {
		return true;
	}

	LocalVector<PendingNode> pending;
	pending.push_back({ p_root.ptr(), 0 });

	while (!pending.is_empty()) {
		const PendingNode current = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);

		if (current.depth >= MAX_DEPTH) {
			return false;
		}

		for (const Ref<RNShadowNode> &child : current.node->children) {
			if (child.is_valid()) {
				pending.push_back({ child.ptr(), current.depth + 1 });
			}
		}
	}

	return true;
}

String RNShadowNode::collect_text() const {
	String result;

	if (view_name == "RCTRawText") {
		result += String(props.get("text", String()));
	}

	for (const Ref<RNShadowNode> &child : children) {
		if (child.is_valid()) {
			result += child->collect_text();
		}
	}

	return result;
}
