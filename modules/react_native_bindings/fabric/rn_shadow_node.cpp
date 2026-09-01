#include "rn_shadow_node.h"

Ref<RNShadowNode> RNShadowNode::clone(bool p_new_children, const Dictionary *p_new_props) const {
	Ref<RNShadowNode> copy;
	copy.instantiate();

	copy->tag = tag;
	copy->view_name = view_name;
	copy->props = p_new_props ? *p_new_props : props;
	copy->layout = layout;
	copy->event_target = event_target;

	if (!p_new_children) {
		// Shares the child subtrees by reference; the renderer replaces only what changed.
		copy->children = children;
	}

	return copy;
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
