#include "fabric_ui_manager.h"

#include "../root_view/react_native_root_view.h"

#include "core/error/error_macros.h"
#include "core/object/object.h"
#include "core/variant/array.h"

#include <string>

const char *FabricUIManager::GLOBAL_NAME = "nativeFabricUIManager";

namespace {

constexpr int MAX_PROP_DEPTH = 8;

String string_from_utf8(const std::string &p_value) {
	return String::utf8(p_value.c_str());
}

Variant jsi_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value, int p_depth) {
	if (p_depth > MAX_PROP_DEPTH) {
		return Variant();
	}

	if (p_value.isBool()) {
		return p_value.getBool();
	}
	if (p_value.isNumber()) {
		return p_value.getNumber();
	}
	if (p_value.isString()) {
		return string_from_utf8(p_value.getString(rt).utf8(rt));
	}
	if (!p_value.isObject()) {
		return Variant();
	}

	facebook::jsi::Object object = p_value.getObject(rt);

	// Functions (event handlers, instanceHandle) have no Variant equivalent and are
	// not something the mounting layer can use yet.
	if (object.isFunction(rt) || object.isHostObject(rt)) {
		return Variant();
	}

	if (object.isArray(rt)) {
		facebook::jsi::Array js_array = object.asArray(rt);
		Array result;
		for (size_t i = 0; i < js_array.size(rt); ++i) {
			result.push_back(jsi_to_variant(rt, js_array.getValueAtIndex(rt, i), p_depth + 1));
		}
		return result;
	}

	Dictionary result;
	facebook::jsi::Array names = object.getPropertyNames(rt);
	for (size_t i = 0; i < names.size(rt); ++i) {
		facebook::jsi::Value key = names.getValueAtIndex(rt, i);
		if (!key.isString()) {
			continue;
		}
		facebook::jsi::String key_string = key.getString(rt);
		result[string_from_utf8(key_string.utf8(rt))] = jsi_to_variant(rt, object.getProperty(rt, key_string), p_depth + 1);
	}
	return result;
}

Dictionary props_from(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	const Variant converted = jsi_to_variant(rt, p_value, 0);
	return converted.get_type() == Variant::DICTIONARY ? Dictionary(converted) : Dictionary();
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

std::shared_ptr<RNChildSetHandle> child_set_from(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	if (!p_value.isObject()) {
		return nullptr;
	}

	facebook::jsi::Object object = p_value.getObject(rt);
	if (!object.isHostObject<RNChildSetHandle>(rt)) {
		return nullptr;
	}

	return object.getHostObject<RNChildSetHandle>(rt);
}

facebook::jsi::Value wrap_node(facebook::jsi::Runtime &rt, const Ref<RNShadowNode> &p_node) {
	return facebook::jsi::Object::createFromHostObject(rt, std::make_shared<RNShadowNodeHandle>(p_node));
}

} //namespace

FabricUIManager::FabricUIManager(ObjectID p_root_view_id) : root_view_id(p_root_view_id) {
}

ReactNativeRootView *FabricUIManager::get_root_view() const {
	return Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(root_view_id));
}

facebook::jsi::Value FabricUIManager::create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 4 || !p_args[0].isNumber() || !p_args[1].isString()) {
		throw facebook::jsi::JSError(rt, std::string("createNode(tag, viewName, rootTag, props, instanceHandle)"));
	}

	Ref<RNShadowNode> node;
	node.instantiate();
	node->tag = static_cast<int>(p_args[0].getNumber());
	node->view_name = string_from_utf8(p_args[1].getString(rt).utf8(rt));
	node->props = props_from(rt, p_args[3]);

	return wrap_node(rt, node);
}

facebook::jsi::Value FabricUIManager::clone_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc, bool p_new_children, bool p_new_props) {
	if (p_argc < 1) {
		throw facebook::jsi::JSError(rt, std::string("cloneNode*(node, [newProps])"));
	}

	const Ref<RNShadowNode> source = node_from(rt, p_args[0]);
	if (source.is_null()) {
		throw facebook::jsi::JSError(rt, std::string("cloneNode*: argument is not a shadow node."));
	}

	if (p_new_props) {
		if (p_argc < 2) {
			throw facebook::jsi::JSError(rt, std::string("cloneNode*WithNewProps: missing props."));
		}
		const Dictionary props = props_from(rt, p_args[1]);
		return wrap_node(rt, source->clone(p_new_children, &props));
	}

	return wrap_node(rt, source->clone(p_new_children, nullptr));
}

facebook::jsi::Value FabricUIManager::create_child_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	(void)p_args;
	(void)p_argc;
	return facebook::jsi::Object::createFromHostObject(rt, std::make_shared<RNChildSetHandle>());
}

facebook::jsi::Value FabricUIManager::append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2) {
		throw facebook::jsi::JSError(rt, std::string("appendChild(parent, child)"));
	}

	const Ref<RNShadowNode> parent = node_from(rt, p_args[0]);
	const Ref<RNShadowNode> child = node_from(rt, p_args[1]);
	if (parent.is_null() || child.is_null()) {
		throw facebook::jsi::JSError(rt, std::string("appendChild: argument is not a shadow node."));
	}

	// Legal because the renderer only ever appends to a node it just cloned; the
	// original's child vector is a separate copy.
	parent->children.push_back(child);
	return facebook::jsi::Value(rt, p_args[0]);
}

facebook::jsi::Value FabricUIManager::append_child_to_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2) {
		throw facebook::jsi::JSError(rt, std::string("appendChildToSet(childSet, child)"));
	}

	std::shared_ptr<RNChildSetHandle> child_set = child_set_from(rt, p_args[0]);
	const Ref<RNShadowNode> child = node_from(rt, p_args[1]);
	if (!child_set || child.is_null()) {
		throw facebook::jsi::JSError(rt, std::string("appendChildToSet: bad argument."));
	}

	child_set->children.push_back(child);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::complete_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2) {
		throw facebook::jsi::JSError(rt, std::string("completeRoot(rootTag, childSet)"));
	}

	std::shared_ptr<RNChildSetHandle> child_set = child_set_from(rt, p_args[1]);
	if (!child_set) {
		throw facebook::jsi::JSError(rt, std::string("completeRoot: second argument is not a child set."));
	}

	ReactNativeRootView *root_view = get_root_view();
	if (!root_view) {
		throw facebook::jsi::JSError(rt, std::string("completeRoot: root view is gone."));
	}

	Ref<RNShadowNode> root;
	root.instantiate();
	root->tag = p_args[0].isNumber() ? static_cast<int>(p_args[0].getNumber()) : 0;
	root->view_name = "RCTRootView";
	root->children = child_set->children;

	// The runtime mutex is held here, so defer scene-tree changes until evaluation ends.
	root_view->call_deferred("mount", root);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(rt);

	auto host_fn = [&](const char *p_fn_name, unsigned int p_arg_count, auto p_body) {
		return facebook::jsi::Function::createFromHostFunction(
				rt, facebook::jsi::PropNameID::forAscii(rt, p_fn_name), p_arg_count,
				[p_body](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) {
					return p_body(rt_inner, args, argc);
				});
	};

	if (name == "createNode") {
		return host_fn("createNode", 5, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return create_node(rt_inner, args, argc);
		});
	}
	if (name == "cloneNode") {
		return host_fn("cloneNode", 1, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return clone_node(rt_inner, args, argc, false, false);
		});
	}
	if (name == "cloneNodeWithNewChildren") {
		return host_fn("cloneNodeWithNewChildren", 1, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return clone_node(rt_inner, args, argc, true, false);
		});
	}
	if (name == "cloneNodeWithNewProps") {
		return host_fn("cloneNodeWithNewProps", 2, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return clone_node(rt_inner, args, argc, false, true);
		});
	}
	if (name == "cloneNodeWithNewChildrenAndProps") {
		return host_fn("cloneNodeWithNewChildrenAndProps", 2, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return clone_node(rt_inner, args, argc, true, true);
		});
	}
	if (name == "createChildSet") {
		return host_fn("createChildSet", 1, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return create_child_set(rt_inner, args, argc);
		});
	}
	if (name == "appendChild") {
		return host_fn("appendChild", 2, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return append_child(rt_inner, args, argc);
		});
	}
	if (name == "appendChildToSet") {
		return host_fn("appendChildToSet", 2, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return append_child_to_set(rt_inner, args, argc);
		});
	}
	if (name == "completeRoot") {
		return host_fn("completeRoot", 2, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return complete_root(rt_inner, args, argc);
		});
	}

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
		return host_fn("unstable_getCurrentEventPriority", 0, [](facebook::jsi::Runtime &, const facebook::jsi::Value *, size_t) {
			return facebook::jsi::Value(EVENT_PRIORITY_DEFAULT);
		});
	}

	// Everything else in the spec: a stub that names itself when called. Unimplemented
	// calls being loud rather than silent is the primary diagnostic instrument when RN
	// boots. It warns on call, not on property access, because JS probes an object for
	// all sorts of properties it never invokes.
	return host_fn(name.c_str(), 0, [name](facebook::jsi::Runtime &, const facebook::jsi::Value *, size_t) {
		WARN_PRINT(String("nativeFabricUIManager.") + string_from_utf8(name) + "() is not implemented.");
		return facebook::jsi::Value::undefined();
	});
}

std::vector<facebook::jsi::PropNameID> FabricUIManager::getPropertyNames(facebook::jsi::Runtime &rt) {
	static const char *NAMES[] = {
		"createNode",
		"cloneNode",
		"cloneNodeWithNewChildren",
		"cloneNodeWithNewProps",
		"cloneNodeWithNewChildrenAndProps",
		"createChildSet",
		"appendChild",
		"appendChildToSet",
		"completeRoot",
		"unstable_DefaultEventPriority",
		"unstable_DiscreteEventPriority",
		"unstable_ContinuousEventPriority",
		"unstable_IdleEventPriority",
		"unstable_getCurrentEventPriority",
	};

	std::vector<facebook::jsi::PropNameID> names;
	for (const char *entry : NAMES) {
		names.push_back(facebook::jsi::PropNameID::forAscii(rt, entry));
	}
	return names;
}
