#include "rn_registry.h"

#include "core/object/object.h"
#include "scene/main/node.h"

int RNRegistry::allocate_tag() {
	return next_tag++;
}

void RNRegistry::register_node(int p_tag, Node *p_node) {
	ERR_FAIL_NULL(p_node);
	nodes[p_tag] = p_node->get_instance_id();
}

void RNRegistry::unregister_node(int p_tag) {
	nodes.erase(p_tag);
}

Node *RNRegistry::get_node(int p_tag) const {
	const ObjectID *id = nodes.getptr(p_tag);
	if (!id) {
		return nullptr;
	}
	return Object::cast_to<Node>(ObjectDB::get_instance(*id));
}

void RNRegistry::clear() {
	nodes.clear();
}
