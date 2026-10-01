#include "fabric_ui_manager.h"

#include "../components/rn_host_descriptor_registry.h"
#include "../interop/rn_value_codec.h"
#include "rn_event_target.h"

#include "core/error/error_macros.h"

#include <cmath>
#include <iterator>
#include <string>
#include <vector>

const char *FabricUIManager::GLOBAL_NAME = "nativeFabricUIManager";

namespace {
String string_from_utf8(const std::string &p_value) {
	return String::utf8(p_value.c_str(), int(p_value.length()));
}

std::string string_to_utf8(const String &p_value) {
	const CharString utf8 = p_value.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

const char *jsi_type(const facebook::jsi::Value &p_value) {
	if (p_value.isUndefined()) {
		return "undefined";
	}
	if (p_value.isNull()) {
		return "null";
	}
	if (p_value.isBool()) {
		return "boolean";
	}
	if (p_value.isNumber()) {
		return "number";
	}
	if (p_value.isString()) {
		return "string";
	}
	if (p_value.isObject()) {
		return "object";
	}
	return "unknown";
}

void argument_error(facebook::jsi::Runtime &p_runtime, const char *p_method, size_t p_index, const char *p_expected, const facebook::jsi::Value *p_args, size_t p_argc) {
	const char *actual = p_index < p_argc ? jsi_type(p_args[p_index]) : "missing";
	throw facebook::jsi::JSError(p_runtime, std::string(p_method) + ": argument " + std::to_string(p_index) + " expected " + p_expected + ", got " + actual + ".");
}

Variant jsi_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value, const String &p_path) {
	if (p_value.isObject() && p_value.getObject(rt).isFunction(rt)) {
		return true;
	}
	RNDecodedValue decoded = RNValueCodec::from_js(rt, p_value, RNValueSchema::value(RNValueType::DYNAMIC), "Fabric", p_path);
	if (!decoded.ok()) {
		throw facebook::jsi::JSError(rt, string_to_utf8(decoded.error.describe()));
	}
	return decoded.value;
}

Dictionary props_from(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	const Variant value = jsi_to_variant(rt, p_value, "props");
	return value.get_type() == Variant::DICTIONARY ? Dictionary(value) : Dictionary();
}

Vector<String> prop_keys(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	Vector<String> result;
	if (!p_value.isObject()) {
		return result;
	}
	facebook::jsi::Array names = p_value.getObject(rt).getPropertyNames(rt);
	for (size_t i = 0; i < names.size(rt); ++i) {
		facebook::jsi::Value key = names.getValueAtIndex(rt, i);
		if (key.isString()) {
			result.push_back(string_from_utf8(key.getString(rt).utf8(rt)));
		}
	}
	return result;
}

void merge_props(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_updates, Dictionary &r_props) {
	if (!p_updates.isObject()) {
		return;
	}
	facebook::jsi::Object updates = p_updates.getObject(rt);
	facebook::jsi::Array names = updates.getPropertyNames(rt);
	for (size_t i = 0; i < names.size(rt); ++i) {
		facebook::jsi::Value key = names.getValueAtIndex(rt, i);
		if (!key.isString()) {
			continue;
		}
		facebook::jsi::String key_string = key.getString(rt);
		const String name = string_from_utf8(key_string.utf8(rt));
		facebook::jsi::Value value = updates.getProperty(rt, key_string);
		if (value.isNull() || value.isUndefined()) {
			r_props.erase(name);
		} else {
			r_props[name] = jsi_to_variant(rt, value, "props." + name);
		}
	}
}

Ref<RNShadowNode> node_from(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	if (!p_value.isObject()) {
		return Ref<RNShadowNode>();
	}
	facebook::jsi::Object object = p_value.getObject(rt);
	if (!object.isHostObject<RNShadowNodeHandle>(rt)) {
		return Ref<RNShadowNode>();
	}
	return object.getHostObject<RNShadowNodeHandle>(rt)->node;
}

bool subtree_contains_tag(const Ref<RNShadowNode> &p_node, int p_tag) {
	if (p_node.is_null()) {
		return false;
	}
	Vector<Ref<RNShadowNode>> pending;
	pending.push_back(p_node);
	while (!pending.is_empty()) {
		Ref<RNShadowNode> current = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		if (current->tag == p_tag) {
			return true;
		}
		for (const Ref<RNShadowNode> &child : current->children) {
			if (child.is_valid()) {
				pending.push_back(child);
			}
		}
	}
	return false;
}

void invalidate_node(const Ref<RNShadowNode> &p_node, const String &p_error) {
	if (p_node.is_valid()) {
		p_node->structurally_valid = false;
		p_node->structural_error = p_error;
	}
}

std::shared_ptr<RNChildSetHandle> child_set_from(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	if (!p_value.isObject()) {
		return nullptr;
	}
	facebook::jsi::Object object = p_value.getObject(rt);
	return object.isHostObject<RNChildSetHandle>(rt) ? object.getHostObject<RNChildSetHandle>(rt) : nullptr;
}

facebook::jsi::Value wrap_node(facebook::jsi::Runtime &rt, const Ref<RNShadowNode> &p_node) {
	return facebook::jsi::Object::createFromHostObject(rt, std::make_shared<RNShadowNodeHandle>(p_node));
}

facebook::jsi::Value variant_to_jsi(facebook::jsi::Runtime &rt, const Variant &p_value) {
	facebook::jsi::Value result;
	RNError error;
	if (!RNValueCodec::to_js(rt, p_value, RNValueSchema::value(RNValueType::DYNAMIC), result, error, nullptr, "Fabric", "payload")) {
		throw facebook::jsi::JSError(rt, string_to_utf8(error.describe()));
	}
	return result;
}

const RNMountedNodeSnapshot *snapshot_node(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state, const Ref<RNShadowNode> &p_node, std::shared_ptr<const RNSurfaceSnapshot> &r_snapshot) {
	if (!p_state || p_node.is_null()) {
		return nullptr;
	}
	auto route = p_state->routes.find(p_node->root_tag);
	if (route == p_state->routes.end() || route->second.runtime_generation != p_node->runtime_generation || route->second.surface_epoch != p_node->surface_epoch) {
		return nullptr;
	}
	auto snapshot = p_state->snapshots.find(p_node->root_tag);
	if (snapshot == p_state->snapshots.end() || !snapshot->second || snapshot->second->runtime_generation != p_node->runtime_generation || snapshot->second->surface_epoch != p_node->surface_epoch) {
		return nullptr;
	}
	r_snapshot = snapshot->second;
	return r_snapshot->nodes.getptr(p_node->tag);
}

bool prepare_imperative_request(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state, const Ref<RNShadowNode> &p_node, RNImperativeRequest &r_request) {
	if (!p_state || p_node.is_null()) {
		return false;
	}
	auto route = p_state->routes.find(p_node->root_tag);
	if (route == p_state->routes.end() || route->second.runtime_generation != p_node->runtime_generation || route->second.surface_epoch != p_node->surface_epoch) {
		return false;
	}
	auto desired = p_state->desired_nodes.find({ p_node->root_tag, p_node->tag });
	if (desired != p_state->desired_nodes.end()) {
		if (desired->second.component_name != p_node->view_name) {
			return false;
		}
		r_request.required_revision = route->second.desired_revision;
	} else {
		auto snapshot = p_state->snapshots.find(p_node->root_tag);
		const RNMountedNodeSnapshot *mounted = snapshot != p_state->snapshots.end() && snapshot->second ? snapshot->second->nodes.getptr(p_node->tag) : nullptr;
		if (!mounted || mounted->view_name != p_node->view_name) {
			return false;
		}
		r_request.required_revision = snapshot->second->revision;
	}
	r_request.runtime_generation = p_node->runtime_generation;
	r_request.root_tag = p_node->root_tag;
	r_request.surface_epoch = p_node->surface_epoch;
	r_request.tag = p_node->tag;
	r_request.component_name = p_node->view_name;
	return true;
}

void index_tree(const Ref<RNShadowNode> &p_node, RNRuntimeCoordinatorState &r_state) {
	if (p_node.is_null()) {
		return;
	}
	RNDesiredNode desired;
	desired.component_name = p_node->view_name;
	desired.event_target = p_node->event_target;
	r_state.desired_nodes[{ p_node->root_tag, p_node->tag }] = desired;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		index_tree(child, r_state);
	}
}

class EventPriorityGuard {
	int &priority;
	int previous;

public:
	EventPriorityGuard(int &p_priority, int p_next) :
			priority(p_priority),
			previous(p_priority) {
		priority = p_next;
	}
	~EventPriorityGuard() { priority = previous; }
};
} // namespace

FabricUIManager::FabricUIManager(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) :
		state(p_state) {
}

void FabricUIManager::register_surface(const RNSurfaceRoute &p_route) {
	if (runtime_generation == 0) {
		runtime_generation = p_route.runtime_generation;
	}
	Ref<RNShadowNode> root;
	root.instantiate();
	root->tag = p_route.root_tag;
	root->root_tag = p_route.root_tag;
	root->runtime_generation = p_route.runtime_generation;
	root->surface_epoch = p_route.surface_epoch;
	root->view_name = "RCTRootView";
	if (auto shared = state.lock(); shared && shared->descriptor_registry) {
		root->descriptor = shared->descriptor_registry->find(root->view_name);
	}
	root->event_target = std::make_shared<RNEventTarget>(root->tag, root->runtime_generation, root->root_tag, root->surface_epoch);
	virtual_roots[p_route.root_tag] = root;
	event_targets[{ p_route.root_tag, root->tag }] = root->event_target;

	auto shared = state.lock();
	if (shared) {
		auto snapshot = std::make_shared<RNSurfaceSnapshot>();
		snapshot->root_tag = p_route.root_tag;
		snapshot->runtime_generation = p_route.runtime_generation;
		snapshot->surface_epoch = p_route.surface_epoch;
		RNMountedNodeSnapshot root_snapshot;
		root_snapshot.tag = p_route.root_tag;
		root_snapshot.view_name = "RCTRootView";
		root_snapshot.shadow_node = root;
		snapshot->nodes[p_route.root_tag] = root_snapshot;
		shared->snapshots[p_route.root_tag] = snapshot;
	}
}

void FabricUIManager::remove_surface(int p_root_tag, uint64_t p_epoch) {
	virtual_roots.erase(p_root_tag);
	responder_tags.erase(p_root_tag);
	for (auto it = event_targets.begin(); it != event_targets.end();) {
		if (it->first.root_tag == p_root_tag) {
			if (auto target = it->second.lock(); target && target->get_surface_epoch() == p_epoch) {
				target->reset();
			}
			it = event_targets.erase(it);
		} else {
			++it;
		}
	}
	if (auto shared = state.lock()) {
		for (auto it = shared->desired_nodes.begin(); it != shared->desired_nodes.end();) {
			if (it->first.root_tag == p_root_tag) {
				it = shared->desired_nodes.erase(it);
			} else {
				++it;
			}
		}
	}
}

void FabricUIManager::reconcile_surface(const RNSurfaceSnapshot &p_snapshot) {
	auto responder = responder_tags.find(p_snapshot.root_tag);
	if (responder != responder_tags.end() && !p_snapshot.nodes.has(responder->second)) {
		responder_tags.erase(responder);
	}
}

facebook::jsi::Value FabricUIManager::link_root_node(facebook::jsi::Runtime &p_runtime, int p_root_tag, const facebook::jsi::Object &p_instance_handle) {
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::undefined();
	}
	auto route = shared->routes.find(p_root_tag);
	auto root = virtual_roots.find(p_root_tag);
	if (route == shared->routes.end() || root == virtual_roots.end() || route->second.runtime_generation != runtime_generation || root->second->surface_epoch != route->second.surface_epoch) {
		return facebook::jsi::Value::undefined();
	}
	root->second->event_target->set_instance_handle(p_runtime, p_instance_handle);
	return wrap_node(p_runtime, root->second);
}

facebook::jsi::Value FabricUIManager::create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 5 || !p_args[0].isNumber()) {
		argument_error(rt, "createNode", 0, "number", p_args, p_argc);
	}
	if (!p_args[1].isString()) {
		argument_error(rt, "createNode", 1, "string", p_args, p_argc);
	}
	if (!p_args[2].isNumber()) {
		argument_error(rt, "createNode", 2, "number", p_args, p_argc);
	}
	if (!p_args[3].isObject() && !p_args[3].isNull()) {
		argument_error(rt, "createNode", 3, "object or null", p_args, p_argc);
	}
	if (!p_args[4].isObject()) {
		argument_error(rt, "createNode", 4, "object", p_args, p_argc);
	}
	const int root_tag = int(p_args[2].getNumber());
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::undefined();
	}
	auto route = shared->routes.find(root_tag);
	if (route == shared->routes.end() || route->second.runtime_generation != runtime_generation || route->second.status == RNSurfaceStatus::DETACHED) {
		return facebook::jsi::Value::undefined();
	}
	Ref<RNShadowNode> node;
	node.instantiate();
	node->tag = int(p_args[0].getNumber());
	node->root_tag = root_tag;
	node->runtime_generation = route->second.runtime_generation;
	node->surface_epoch = route->second.surface_epoch;
	node->view_name = string_from_utf8(p_args[1].getString(rt).utf8(rt));
	if (!shared->descriptor_registry) {
		throw facebook::jsi::JSError(rt, "E_NATIVE: host descriptor registry is unavailable.");
	}
	node->descriptor = shared->descriptor_registry->find(node->view_name);
	if (!node->descriptor) {
		throw facebook::jsi::JSError(rt, string_to_utf8(vformat("E_UNKNOWN_COMPONENT [createNode]: no Godot host descriptor is registered for '%s'.", node->view_name)));
	}
	auto existing_target = event_targets.find({ root_tag, node->tag });
	if (existing_target != event_targets.end() && !existing_target->second.expired()) {
		throw facebook::jsi::JSError(rt, "createNode: duplicate live tag in this surface.");
	}
	node->props = props_from(rt, p_args[3]);
	node->event_target = std::make_shared<RNEventTarget>(node->tag, node->runtime_generation, root_tag, node->surface_epoch, rt, p_args[4].getObject(rt));
	event_targets[{ root_tag, node->tag }] = node->event_target;
	return wrap_node(rt, node);
}

facebook::jsi::Value FabricUIManager::clone_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc, bool p_new_children, bool p_new_props) {
	if (p_argc < 1) {
		argument_error(rt, "cloneNode", 0, "shadow node", p_args, p_argc);
	}
	const Ref<RNShadowNode> source = node_from(rt, p_args[0]);
	if (source.is_null()) {
		argument_error(rt, "cloneNode", 0, "shadow node", p_args, p_argc);
	}
	if (p_new_props) {
		if (p_argc < 2 || !p_args[1].isObject()) {
			argument_error(rt, "cloneNodeWithNewProps", 1, "object", p_args, p_argc);
		}
		Dictionary props = source->props.duplicate();
		merge_props(rt, p_args[1], props);
		Ref<RNShadowNode> result = source->clone(p_new_children, &props);
		const uint64_t prop_revision = next_prop_revision++;
		for (const String &key : prop_keys(rt, p_args[1])) {
			result->declarative_prop_revisions[key] = prop_revision;
		}
		return wrap_node(rt, result);
	}
	return wrap_node(rt, source->clone(p_new_children, nullptr));
}

facebook::jsi::Value FabricUIManager::create_child_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	auto child_set = std::make_shared<RNChildSetHandle>();
	if (p_argc > 0) {
		if (!p_args[0].isNumber()) {
			argument_error(rt, "createChildSet", 0, "number", p_args, p_argc);
		}
		const int root_tag = int(p_args[0].getNumber());
		auto shared = state.lock();
		if (!shared) {
			return facebook::jsi::Value::undefined();
		}
		auto route = shared->routes.find(root_tag);
		if (route == shared->routes.end()) {
			return facebook::jsi::Value::undefined();
		}
		child_set->root_tag = root_tag;
		child_set->runtime_generation = route->second.runtime_generation;
		child_set->surface_epoch = route->second.surface_epoch;
	}
	return facebook::jsi::Object::createFromHostObject(rt, child_set);
}

facebook::jsi::Value FabricUIManager::append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2) {
		argument_error(rt, "appendChild", 1, "shadow node", p_args, p_argc);
	}
	const Ref<RNShadowNode> parent = node_from(rt, p_args[0]);
	const Ref<RNShadowNode> child = node_from(rt, p_args[1]);
	if (parent.is_null()) {
		argument_error(rt, "appendChild", 0, "shadow node", p_args, p_argc);
	}
	if (child.is_null()) {
		argument_error(rt, "appendChild", 1, "shadow node", p_args, p_argc);
	}
	if (parent->root_tag != child->root_tag || parent->runtime_generation != child->runtime_generation || parent->surface_epoch != child->surface_epoch) {
		invalidate_node(parent, "appendChild: cross-surface nodes are not allowed.");
		throw facebook::jsi::JSError(rt, "appendChild: cross-surface nodes are not allowed.");
	}
	for (const Ref<RNShadowNode> &existing : parent->children) {
		if (existing.is_valid() && existing->tag == child->tag) {
			invalidate_node(parent, "appendChild: repeated direct child.");
			throw facebook::jsi::JSError(rt, "appendChild: repeated direct child.");
		}
	}
	if (subtree_contains_tag(child, parent->tag)) {
		invalidate_node(parent, "appendChild: cycle detected.");
		throw facebook::jsi::JSError(rt, "appendChild: cycle detected.");
	}
	if (child->validated_depth >= RNShadowNode::MAX_DEPTH) {
		invalidate_node(parent, "appendChild: native depth limit exceeded.");
		throw facebook::jsi::JSError(rt, "appendChild: native depth limit exceeded.");
	}
	parent->children.push_back(child);
	parent->validated_depth = MAX(parent->validated_depth, child->validated_depth + 1);
	return facebook::jsi::Value(rt, p_args[0]);
}

facebook::jsi::Value FabricUIManager::append_child_to_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2) {
		argument_error(rt, "appendChildToSet", 1, "shadow node", p_args, p_argc);
	}
	auto child_set = child_set_from(rt, p_args[0]);
	const Ref<RNShadowNode> child = node_from(rt, p_args[1]);
	if (!child_set) {
		argument_error(rt, "appendChildToSet", 0, "child set", p_args, p_argc);
	}
	if (child.is_null()) {
		argument_error(rt, "appendChildToSet", 1, "shadow node", p_args, p_argc);
	}
	if (child_set->root_tag == 0) {
		child_set->root_tag = child->root_tag;
		child_set->runtime_generation = child->runtime_generation;
		child_set->surface_epoch = child->surface_epoch;
	}
	if (child_set->root_tag != child->root_tag || child_set->runtime_generation != child->runtime_generation || child_set->surface_epoch != child->surface_epoch) {
		child_set->structurally_valid = false;
		child_set->structural_error = "appendChildToSet: cross-surface nodes are not allowed.";
		throw facebook::jsi::JSError(rt, "appendChildToSet: cross-surface nodes are not allowed.");
	}
	for (const Ref<RNShadowNode> &existing : child_set->children) {
		if (existing.is_valid() && existing->tag == child->tag) {
			child_set->structurally_valid = false;
			child_set->structural_error = "appendChildToSet: repeated direct child.";
			throw facebook::jsi::JSError(rt, "appendChildToSet: repeated direct child.");
		}
	}
	child_set->children.push_back(child);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::complete_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2 || !p_args[0].isNumber()) {
		argument_error(rt, "completeRoot", 0, "number", p_args, p_argc);
	}
	auto child_set = child_set_from(rt, p_args[1]);
	if (!child_set) {
		argument_error(rt, "completeRoot", 1, "child set", p_args, p_argc);
	}
	const int root_tag = int(p_args[0].getNumber());
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::undefined();
	}
	auto route = shared->routes.find(root_tag);
	auto root_entry = virtual_roots.find(root_tag);
	if (route == shared->routes.end() || root_entry == virtual_roots.end()) {
		return facebook::jsi::Value::undefined();
	}
	if (child_set->root_tag == 0) {
		child_set->root_tag = root_tag;
		child_set->runtime_generation = route->second.runtime_generation;
		child_set->surface_epoch = route->second.surface_epoch;
	}
	if (child_set->root_tag != root_tag || child_set->runtime_generation != route->second.runtime_generation || child_set->surface_epoch != route->second.surface_epoch) {
		throw facebook::jsi::JSError(rt, "completeRoot: child set belongs to another surface.");
	}
	if (!child_set->structurally_valid) {
		throw facebook::jsi::JSError(rt, std::string("completeRoot: rejected invalid child set: ") + string_to_utf8(child_set->structural_error));
	}
	Ref<RNShadowNode> root = root_entry->second->clone(true, nullptr);
	root->children = child_set->children;
	if (!RNShadowNode::is_within_depth_limit(root)) {
		throw facebook::jsi::JSError(rt, "completeRoot: shadow tree exceeds the native depth limit.");
	}
	root_entry->second = root;
	const uint64_t revision = ++route->second.desired_revision;
	for (auto it = shared->desired_nodes.begin(); it != shared->desired_nodes.end();) {
		if (it->first.root_tag == root_tag) {
			it = shared->desired_nodes.erase(it);
		} else {
			++it;
		}
	}
	index_tree(root, *shared);
	RNPendingCommit commit;
	commit.runtime_generation = route->second.runtime_generation;
	commit.root_tag = root_tag;
	commit.surface_epoch = route->second.surface_epoch;
	commit.revision = revision;
	commit.tree = root;
	RNSurfaceOperation operation;
	operation.kind = RNSurfaceOperationKind::COMMIT;
	operation.commit = commit;
	shared->operation_queues[root_tag].push_back(operation);
	for (auto it = event_targets.begin(); it != event_targets.end();) {
		it = it->second.expired() ? event_targets.erase(it) : std::next(it);
	}
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::register_event_handler(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 1 || !p_args[0].isObject() || !p_args[0].getObject(rt).isFunction(rt)) {
		argument_error(rt, "registerEventHandler", 0, "function", p_args, p_argc);
	}
	event_handler = std::make_unique<facebook::jsi::Function>(p_args[0].getObject(rt).getFunction(rt));
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::set_is_js_responder(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 3 || !p_args[1].isBool() || !p_args[2].isBool()) {
		throw facebook::jsi::JSError(rt, "setIsJSResponder(node, isResponder, blockNativeResponder)");
	}
	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	if (node.is_null()) {
		argument_error(rt, "setIsJSResponder", 0, "shadow node", p_args, p_argc);
	}
	if (p_args[1].getBool()) {
		responder_tags[node->root_tag] = node->tag;
	} else if (get_responder_tag(node->root_tag) == node->tag) {
		responder_tags.erase(node->root_tag);
	}
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::measure(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 2 || !p_args[1].isObject() || !p_args[1].getObject(rt).isFunction(rt)) {
		throw facebook::jsi::JSError(rt, "measure(node, callback)");
	}
	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	std::shared_ptr<const RNSurfaceSnapshot> snapshot;
	const RNMountedNodeSnapshot *value = snapshot_node(state.lock(), node, snapshot);
	const Rect2 local = value ? value->local_rect : Rect2();
	const Point2 page = value ? value->root_rect.position : Point2();
	facebook::jsi::Value args[] = { double(local.position.x), double(local.position.y), double(local.size.x), double(local.size.y), double(page.x), double(page.y) };
	p_args[1].getObject(rt).getFunction(rt).call(rt, static_cast<const facebook::jsi::Value *>(args), size_t(6));
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::measure_in_window(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 2 || !p_args[1].isObject() || !p_args[1].getObject(rt).isFunction(rt)) {
		throw facebook::jsi::JSError(rt, "measureInWindow(node, callback)");
	}
	std::shared_ptr<const RNSurfaceSnapshot> snapshot;
	const RNMountedNodeSnapshot *value = snapshot_node(state.lock(), node_from(rt, p_args[0]), snapshot);
	const Rect2 rect = value ? value->window_rect : Rect2();
	facebook::jsi::Value args[] = { double(rect.position.x), double(rect.position.y), double(rect.size.x), double(rect.size.y) };
	p_args[1].getObject(rt).getFunction(rt).call(rt, static_cast<const facebook::jsi::Value *>(args), size_t(4));
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::measure_layout(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 4) {
		throw facebook::jsi::JSError(rt, "measureLayout(node, relativeNode, onFail, onSuccess)");
	}
	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	const Ref<RNShadowNode> relative = node_from(rt, p_args[1]);
	if (!p_args[2].isObject() || !p_args[2].getObject(rt).isFunction(rt) || !p_args[3].isObject() || !p_args[3].getObject(rt).isFunction(rt)) {
		throw facebook::jsi::JSError(rt, "measureLayout callbacks must be functions.");
	}
	std::shared_ptr<const RNSurfaceSnapshot> snapshot;
	const RNMountedNodeSnapshot *value = snapshot_node(state.lock(), node, snapshot);
	std::shared_ptr<const RNSurfaceSnapshot> relative_snapshot;
	const RNMountedNodeSnapshot *relative_value = snapshot_node(state.lock(), relative, relative_snapshot);
	bool ancestor = false;
	if (value && relative_value && snapshot == relative_snapshot) {
		for (int current = value->tag; current != 0;) {
			if (current == relative->tag) {
				ancestor = true;
				break;
			}
			const RNMountedNodeSnapshot *entry = snapshot->nodes.getptr(current);
			current = entry ? entry->parent_tag : 0;
		}
	}
	if (!ancestor) {
		p_args[2].getObject(rt).getFunction(rt).call(rt);
		return facebook::jsi::Value::undefined();
	}
	const Point2 position = value->root_rect.position - relative_value->root_rect.position;
	facebook::jsi::Value args[] = { double(position.x), double(position.y), double(value->root_rect.size.x), double(value->root_rect.size.y) };
	p_args[3].getObject(rt).getFunction(rt).call(rt, static_cast<const facebook::jsi::Value *>(args), size_t(4));
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::get_bounding_client_rect(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2 || !p_args[1].isBool()) {
		throw facebook::jsi::JSError(rt, "getBoundingClientRect(node, includeTransform)");
	}
	std::shared_ptr<const RNSurfaceSnapshot> snapshot;
	const RNMountedNodeSnapshot *value = snapshot_node(state.lock(), node_from(rt, p_args[0]), snapshot);
	if (!value) {
		return facebook::jsi::Value::null();
	}
	facebook::jsi::Array result(rt, 4);
	result.setValueAtIndex(rt, 0, value->window_rect.position.x);
	result.setValueAtIndex(rt, 1, value->window_rect.position.y);
	result.setValueAtIndex(rt, 2, value->window_rect.size.x);
	result.setValueAtIndex(rt, 3, value->window_rect.size.y);
	return result;
}

facebook::jsi::Value FabricUIManager::set_native_props(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2 || !p_args[1].isObject()) {
		argument_error(rt, "setNativeProps", 1, "object", p_args, p_argc);
	}
	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	if (node.is_null()) {
		argument_error(rt, "setNativeProps", 0, "shadow node", p_args, p_argc);
	}
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::undefined();
	}
	RNImperativeRequest request;
	if (!prepare_imperative_request(shared, node, request)) {
		return facebook::jsi::Value::undefined();
	}
	request.kind = RNImperativeRequestKind::DIRECT_PROPS;
	request.payload = props_from(rt, p_args[1]);
	RNSurfaceOperation operation;
	operation.kind = RNSurfaceOperationKind::IMPERATIVE;
	operation.imperative = request;
	shared->operation_queues[request.root_tag].push_back(operation);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::dispatch_command(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 3 || !p_args[1].isString()) {
		argument_error(rt, "dispatchCommand", 1, "string", p_args, p_argc);
	}
	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	if (node.is_null()) {
		argument_error(rt, "dispatchCommand", 0, "shadow node", p_args, p_argc);
	}
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::undefined();
	}
	RNImperativeRequest request;
	if (!prepare_imperative_request(shared, node, request)) {
		return facebook::jsi::Value::undefined();
	}
	request.kind = RNImperativeRequestKind::COMMAND;
	request.command_name = string_from_utf8(p_args[1].getString(rt).utf8(rt));
	request.payload = jsi_to_variant(rt, p_args[2], "command.args");
	RNSurfaceOperation operation;
	operation.kind = RNSurfaceOperationKind::IMPERATIVE;
	operation.imperative = request;
	shared->operation_queues[request.root_tag].push_back(operation);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::report_surface_error(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2 || !p_args[0].isNumber() || !p_args[1].isString()) {
		throw facebook::jsi::JSError(rt, "__godotReportSurfaceError(rootTag, message, componentStack)");
	}
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::undefined();
	}
	const int root_tag = int(p_args[0].getNumber());
	auto route = shared->routes.find(root_tag);
	if (route != shared->routes.end()) {
		route->second.status = RNSurfaceStatus::FAILED;
		route->second.error = string_from_utf8(p_args[1].getString(rt).utf8(rt));
		if (p_argc > 2 && p_args[2].isString()) {
			route->second.error += "\n" + string_from_utf8(p_args[2].getString(rt).utf8(rt));
		}
		WARN_PRINT(vformat("ReactNativeRootView %d failed inside its error boundary: %s", root_tag, route->second.error));
	}
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::find_shadow_node_by_tag(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 1 || !p_args[0].isNumber() || !std::isfinite(p_args[0].getNumber()) || std::trunc(p_args[0].getNumber()) != p_args[0].getNumber()) {
		return facebook::jsi::Value::null();
	}
	const int tag = int(p_args[0].getNumber());
	auto shared = state.lock();
	if (!shared) {
		return facebook::jsi::Value::null();
	}
	for (const auto &entry : shared->snapshots) {
		if (entry.second) {
			const RNMountedNodeSnapshot *node = entry.second->nodes.getptr(tag);
			if (node && node->shadow_node.is_valid()) {
				return wrap_node(rt, node->shadow_node);
			}
		}
	}
	return facebook::jsi::Value::null();
}

void FabricUIManager::dispatch_event_locked(facebook::jsi::Runtime &p_runtime, const RNNativeEvent &p_event, uint64_t p_generation) {
	auto shared = state.lock();
	if (!shared || !event_handler || p_event.generation != p_generation || p_event.generation != runtime_generation) {
		return;
	}
	auto route = shared->routes.find(p_event.root_tag);
	if (route == shared->routes.end() || route->second.runtime_generation != p_event.generation || route->second.surface_epoch != p_event.surface_epoch) {
		return;
	}
	auto snapshot = shared->snapshots.find(p_event.root_tag);
	const bool retained_removed_target = bool(p_event.retained_target);
	if (snapshot == shared->snapshots.end() || !snapshot->second || (!retained_removed_target && !snapshot->second->nodes.has(p_event.tag))) {
		return;
	}
	std::shared_ptr<RNEventTarget> target = p_event.retained_target;
	if (!target) {
		auto target_entry = event_targets.find({ p_event.root_tag, p_event.tag });
		if (target_entry == event_targets.end()) {
			return;
		}
		target = target_entry->second.lock();
	}
	if (!target || target->get_tag() != p_event.tag || target->get_root_tag() != p_event.root_tag || target->get_generation() != p_generation || target->get_surface_epoch() != p_event.surface_epoch) {
		return;
	}
	facebook::jsi::Value instance_handle = target->lock(p_runtime);
	if (!instance_handle.isObject()) {
		return;
	}
	Dictionary payload = p_event.payload.duplicate(true);
	payload["target"] = p_event.tag;
	EventPriorityGuard guard(current_event_priority, p_event.priority);
	try {
		facebook::jsi::Value args[] = { facebook::jsi::Value(p_runtime, instance_handle), facebook::jsi::String::createFromUtf8(p_runtime, string_to_utf8(p_event.name)), variant_to_jsi(p_runtime, payload) };
		event_handler->call(p_runtime, static_cast<const facebook::jsi::Value *>(args), size_t(3));
	} catch (const facebook::jsi::JSIException &p_error) {
		route->second.error = vformat("React event %s for tag %d failed: %s", p_event.name, p_event.tag, string_from_utf8(p_error.what()));
		WARN_PRINT(route->second.error);
	}
}

void FabricUIManager::dispatch_queued_events_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) {
	auto shared = state.lock();
	if (!shared) {
		return;
	}
	const size_t count = shared->event_queue.size();
	for (size_t i = 0; i < count; ++i) {
		RNNativeEvent event = shared->event_queue.front();
		shared->event_queue.pop_front();
		Vector<RNNativeEvent> capture_events = shared->pointer_capture.apply_pending(event);
		for (const RNNativeEvent &capture : capture_events) {
			dispatch_event_locked(p_runtime, capture, p_generation);
		}
		const int captured = shared->pointer_capture.captured_target(event.root_tag, event.surface_epoch, int(event.payload.get("pointerId", 0)));
		if (captured != 0 && (event.name == "topPointerMove" || event.name == "topPointerUp" || event.name == "topPointerCancel")) {
			event.tag = captured;
		}
		dispatch_event_locked(p_runtime, event, p_generation);
		shared->pointer_capture.observe(event);
		if (event.name == "topPointerUp" || event.name == "topPointerCancel") {
			Vector<RNNativeEvent> released = shared->pointer_capture.apply_pending(event);
			for (const RNNativeEvent &capture : released) {
				dispatch_event_locked(p_runtime, capture, p_generation);
			}
			shared->pointer_capture.finish(event.root_tag, event.surface_epoch, int(event.payload.get("pointerId", 0)));
		}
	}
}

void FabricUIManager::before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) {
	(void)p_runtime;
	event_handler.reset();
	for (auto &entry : event_targets) {
		if (auto target = entry.second.lock()) {
			target->reset();
		}
	}
	event_targets.clear();
	virtual_roots.clear();
	responder_tags.clear();
	current_event_priority = EVENT_PRIORITY_DEFAULT;
	if (auto shared = state.lock()) {
		shared->event_queue.clear();
		shared->operation_queues.clear();
		shared->desired_nodes.clear();
		shared->snapshots.clear();
		shared->pointer_capture.clear();
		for (auto &entry : shared->routes) {
			if (entry.second.runtime_generation == p_generation && entry.second.status != RNSurfaceStatus::DETACHED) {
				entry.second.status = RNSurfaceStatus::FAILED;
				entry.second.error = "Hermes runtime reset invalidated this surface.";
			}
		}
	}
	runtime_generation = p_generation + 1;
}

int FabricUIManager::get_responder_tag(int p_root_tag) const {
	auto found = responder_tags.find(p_root_tag);
	return found == responder_tags.end() ? 0 : found->second;
}

facebook::jsi::Value FabricUIManager::get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(rt);
	auto host_fn = [&](const char *p_fn_name, unsigned int p_arg_count, auto p_body) {
		return facebook::jsi::Function::createFromHostFunction(rt, facebook::jsi::PropNameID::forAscii(rt, p_fn_name), p_arg_count, [p_body](facebook::jsi::Runtime &inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) { return p_body(inner, args, argc); });
	};
#define RN_HOST_METHOD(js_name, count, method) \
	if (name == js_name) \
	return host_fn(js_name, count, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) { return method(inner, args, argc); })
	RN_HOST_METHOD("createNode", 5, create_node);
	if (name == "cloneNode") {
		return host_fn("cloneNode", 1, [this](auto &inner, const auto *args, size_t argc) { return clone_node(inner, args, argc, false, false); });
	}
	if (name == "cloneNodeWithNewChildren") {
		return host_fn("cloneNodeWithNewChildren", 1, [this](auto &inner, const auto *args, size_t argc) { return clone_node(inner, args, argc, true, false); });
	}
	if (name == "cloneNodeWithNewProps") {
		return host_fn("cloneNodeWithNewProps", 2, [this](auto &inner, const auto *args, size_t argc) { return clone_node(inner, args, argc, false, true); });
	}
	if (name == "cloneNodeWithNewChildrenAndProps") {
		return host_fn("cloneNodeWithNewChildrenAndProps", 2, [this](auto &inner, const auto *args, size_t argc) { return clone_node(inner, args, argc, true, true); });
	}
	RN_HOST_METHOD("createChildSet", 1, create_child_set);
	RN_HOST_METHOD("appendChild", 2, append_child);
	RN_HOST_METHOD("appendChildToSet", 2, append_child_to_set);
	RN_HOST_METHOD("completeRoot", 2, complete_root);
	RN_HOST_METHOD("registerEventHandler", 1, register_event_handler);
	RN_HOST_METHOD("setIsJSResponder", 3, set_is_js_responder);
	RN_HOST_METHOD("measure", 2, measure);
	RN_HOST_METHOD("measureInWindow", 2, measure_in_window);
	RN_HOST_METHOD("measureLayout", 4, measure_layout);
	RN_HOST_METHOD("getBoundingClientRect", 2, get_bounding_client_rect);
	RN_HOST_METHOD("setNativeProps", 2, set_native_props);
	RN_HOST_METHOD("dispatchCommand", 3, dispatch_command);
	RN_HOST_METHOD("__godotReportSurfaceError", 3, report_surface_error);
	RN_HOST_METHOD("findShadowNodeByTag_DEPRECATED", 1, find_shadow_node_by_tag);
#undef RN_HOST_METHOD
	if (name == "unstable_DefaultEventPriority") {
		return facebook::jsi::Value(EVENT_PRIORITY_DEFAULT);
	}
	if (name == "unstable_DiscreteEventPriority") {
		return facebook::jsi::Value(EVENT_PRIORITY_DISCRETE);
	}
	if (name == "unstable_ContinuousEventPriority") {
		return facebook::jsi::Value(EVENT_PRIORITY_CONTINUOUS);
	}
	if (name == "unstable_IdleEventPriority") {
		return facebook::jsi::Value(EVENT_PRIORITY_IDLE);
	}
	if (name == "unstable_getCurrentEventPriority") {
		return host_fn("unstable_getCurrentEventPriority", 0, [this](auto &, const auto *, size_t) { return facebook::jsi::Value(current_event_priority); });
	}
	return host_fn(name.c_str(), 0, [name](auto &, const auto *, size_t) { WARN_PRINT(String("nativeFabricUIManager.") + string_from_utf8(name) + "() is not implemented."); return facebook::jsi::Value::undefined(); });
}

std::vector<facebook::jsi::PropNameID> FabricUIManager::getPropertyNames(facebook::jsi::Runtime &rt) {
	static const char *NAMES[] = { "createNode", "cloneNode", "cloneNodeWithNewChildren", "cloneNodeWithNewProps", "cloneNodeWithNewChildrenAndProps", "createChildSet", "appendChild", "appendChildToSet", "completeRoot", "registerEventHandler", "setIsJSResponder", "measure", "measureInWindow", "measureLayout", "getBoundingClientRect", "setNativeProps", "dispatchCommand", "findShadowNodeByTag_DEPRECATED", "__godotReportSurfaceError", "unstable_DefaultEventPriority", "unstable_DiscreteEventPriority", "unstable_ContinuousEventPriority", "unstable_IdleEventPriority", "unstable_getCurrentEventPriority" };
	std::vector<facebook::jsi::PropNameID> names;
	for (const char *entry : NAMES) {
		names.push_back(facebook::jsi::PropNameID::forAscii(rt, entry));
	}
	return names;
}
