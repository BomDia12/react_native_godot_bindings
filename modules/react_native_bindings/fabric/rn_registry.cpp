#include "rn_registry.h"

#include "core/object/object.h"
#include "scene/main/node.h"

int RNRegistry::allocate_tag() {
	return next_tag++;
}

void RNRegistry::register_node(int p_tag, Node *p_node, const Ref<RNShadowNode> &p_shadow_node) {
	ERR_FAIL_NULL(p_node);
	const ObjectID id = p_node->get_instance_id();
	nodes[p_tag] = id;
	tags[id] = p_tag;
	shadow_nodes[p_tag] = p_shadow_node;
}

void RNRegistry::unregister_node(int p_tag) {
	const ObjectID *id = nodes.getptr(p_tag);
	if (id) {
		tags.erase(*id);
	}
	nodes.erase(p_tag);
	shadow_nodes.erase(p_tag);
}

Node *RNRegistry::get_node(int p_tag) const {
	const ObjectID *id = nodes.getptr(p_tag);
	if (!id) {
		return nullptr;
	}
	return Object::cast_to<Node>(ObjectDB::get_instance(*id));
}

int RNRegistry::get_tag(ObjectID p_id) const {
	const int *tag = tags.getptr(p_id);
	return tag ? *tag : 0;
}

Ref<RNShadowNode> RNRegistry::get_shadow_node(int p_tag) const {
	const Ref<RNShadowNode> *node = shadow_nodes.getptr(p_tag);
	return node ? *node : Ref<RNShadowNode>();
}

bool RNRegistry::has_tag(int p_tag) const {
	return shadow_nodes.has(p_tag);
}

void RNRegistry::clear() {
	nodes.clear();
	tags.clear();
	shadow_nodes.clear();
}
