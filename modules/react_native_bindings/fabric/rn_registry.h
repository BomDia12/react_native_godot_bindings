#pragma once

#include "rn_shadow_node.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"

class Node;

// Tag allocator and tag -> scene node map.
//
// Nodes are stored as ObjectID, never as a raw Node *: a jsi::HostObject can outlive
// the scene node it refers to (JS holds it until GC), so a stale handle must degrade
// to a null check instead of a dangling pointer.
class RNRegistry {
	int next_tag = 1;
	HashMap<int, ObjectID> nodes;
	HashMap<ObjectID, int> tags;
	HashMap<int, Ref<RNShadowNode>> shadow_nodes;

public:
	int allocate_tag();

	void register_node(int p_tag, Node *p_node, const Ref<RNShadowNode> &p_shadow_node);
	void unregister_node(int p_tag);

	// Returns nullptr if the tag is unknown or the node has been freed.
	Node *get_node(int p_tag) const;
	int get_tag(ObjectID p_id) const;
	Ref<RNShadowNode> get_shadow_node(int p_tag) const;
	bool has_tag(int p_tag) const;

	void clear();
};
