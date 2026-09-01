#include "fabric_ui_manager.h"

#include "../root_view/react_native_root_view.h"
#include "rn_event_target.h"

#include "core/error/error_macros.h"
#include "core/object/object.h"
#include "core/variant/array.h"

#include <iterator>
#include <string>
#include <vector>

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

// Fabric's prop diffs spell "this prop went away" as an explicit null, so a null erases
// rather than overwrites. The distinction matters because jsi_to_variant also maps
// functions to NIL, and a live onLayout handler must stay present in the dictionary.
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
			r_props[name] = jsi_to_variant(rt, value, 0);
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

std::string string_to_utf8(const String &p_value) {
	const CharString utf8 = p_value.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

facebook::jsi::Value variant_to_jsi(facebook::jsi::Runtime &rt, const Variant &p_value, int p_depth = 0) {
	if (p_depth > MAX_PROP_DEPTH) {
		return facebook::jsi::Value::undefined();
	}

	switch (p_value.get_type()) {
		case Variant::NIL:
			return facebook::jsi::Value::null();
		case Variant::BOOL:
			return facebook::jsi::Value(bool(p_value));
		case Variant::INT:
			return facebook::jsi::Value(double(int64_t(p_value)));
		case Variant::FLOAT:
			return facebook::jsi::Value(double(p_value));
		case Variant::STRING:
		case Variant::STRING_NAME: {
			return facebook::jsi::String::createFromUtf8(rt, string_to_utf8(String(p_value)));
		}
		case Variant::ARRAY: {
			const Array array = p_value;
			facebook::jsi::Array result(rt, array.size());
			for (int i = 0; i < array.size(); ++i) {
				result.setValueAtIndex(rt, i, variant_to_jsi(rt, array[i], p_depth + 1));
			}
			return result;
		}
		case Variant::DICTIONARY: {
			const Dictionary dictionary = p_value;
			facebook::jsi::Object result(rt);
			const Array keys = dictionary.keys();
			for (int i = 0; i < keys.size(); ++i) {
				const String key = keys[i];
				result.setProperty(rt, string_to_utf8(key).c_str(), variant_to_jsi(rt, dictionary[keys[i]], p_depth + 1));
			}
			return result;
		}
		default:
			return facebook::jsi::Value::undefined();
	}
}

} //namespace

FabricUIManager::FabricUIManager(ObjectID p_root_view_id, uint64_t p_runtime_generation) :
		root_view_id(p_root_view_id), runtime_generation(p_runtime_generation) {
}

ReactNativeRootView *FabricUIManager::get_root_view() const {
	return Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(root_view_id));
}

facebook::jsi::Value FabricUIManager::create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 5 || !p_args[0].isNumber() || !p_args[1].isString() || !p_args[4].isObject()) {
		throw facebook::jsi::JSError(rt, std::string("createNode(tag, viewName, rootTag, props, instanceHandle)"));
	}

	Ref<RNShadowNode> node;
	node.instantiate();
	node->tag = static_cast<int>(p_args[0].getNumber());
	node->view_name = string_from_utf8(p_args[1].getString(rt).utf8(rt));
	node->props = props_from(rt, p_args[3]);
	node->event_target = std::make_shared<RNEventTarget>(node->tag, runtime_generation, rt, p_args[4].getObject(rt));
	event_targets[node->tag] = node->event_target;

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
		Dictionary props = source->props.duplicate(true);
		merge_props(rt, p_args[1], props);
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

	// Nodes React has dropped take their event target with them. Pruning here, once per
	// commit, is what keeps the map from growing for the lifetime of the runtime.
	for (auto entry = event_targets.begin(); entry != event_targets.end();) {
		entry = entry->second.expired() ? event_targets.erase(entry) : std::next(entry);
	}

	root_view->enqueue_mount(root, runtime_generation);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::register_event_handler(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 1 || !p_args[0].isObject() || !p_args[0].getObject(rt).isFunction(rt)) {
		throw facebook::jsi::JSError(rt, std::string("registerEventHandler(handler) expects one function."));
	}
	event_handler = std::make_unique<facebook::jsi::Function>(p_args[0].getObject(rt).getFunction(rt));
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::set_is_js_responder(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 3 || !p_args[1].isBool() || !p_args[2].isBool()) {
		throw facebook::jsi::JSError(rt, std::string("setIsJSResponder(node, isResponder, blockNativeResponder)"));
	}

	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	if (node.is_null()) {
		throw facebook::jsi::JSError(rt, std::string("setIsJSResponder: first argument is not a shadow node."));
	}

	// Deliberately not checking that the node is still mounted: React releases the
	// responder for nodes that unmounted mid-gesture, and throwing there would abort
	// the rest of the dispatch and strand ResponderEventPlugin holding the responder.
	if (p_args[1].getBool()) {
		responder_tag = node->tag;
	} else if (responder_tag == node->tag) {
		responder_tag = 0;
	}
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value FabricUIManager::measure(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc != 2 || !p_args[1].isObject() || !p_args[1].getObject(rt).isFunction(rt)) {
		throw facebook::jsi::JSError(rt, std::string("measure(node, callback)"));
	}

	const Ref<RNShadowNode> node = node_from(rt, p_args[0]);
	ReactNativeRootView *root_view = get_root_view();
	Rect2 local_rect;
	Point2 page_position;
	// An unmounted or not-yet-mounted node still gets its callback, with zeros: React
	// treats measure() as always resolving, and a commit is a frame behind the JS that
	// asks for it, so refusing to answer strands the caller forever.
	if (!node.is_null() && root_view) {
		root_view->get_measurement(node->tag, local_rect, page_position);
	}

	facebook::jsi::Function callback = p_args[1].getObject(rt).getFunction(rt);
	facebook::jsi::Value values[] = {
		facebook::jsi::Value(double(local_rect.position.x)),
		facebook::jsi::Value(double(local_rect.position.y)),
		facebook::jsi::Value(double(local_rect.size.x)),
		facebook::jsi::Value(double(local_rect.size.y)),
		facebook::jsi::Value(double(page_position.x)),
		facebook::jsi::Value(double(page_position.y)),
	};
	callback.call(rt, static_cast<const facebook::jsi::Value *>(values), size_t(6));
	return facebook::jsi::Value::undefined();
}

void FabricUIManager::enqueue_event(const RNNativeEvent &p_event) {
	event_queue.push_back(p_event);
}

void FabricUIManager::request_event_flush() {
	if (event_flush_scheduled || event_queue.empty()) {
		return;
	}
	event_flush_scheduled = true;
	if (ReactNativeRootView *root_view = get_root_view()) {
		root_view->call_deferred("_flush_native_events");
	}
}

void FabricUIManager::clear_events() {
	event_queue.clear();
	event_flush_scheduled = false;
}

void FabricUIManager::dispatch_queued_events_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) {
	event_flush_scheduled = false;
	while (!event_queue.empty()) {
		RNNativeEvent event = event_queue.front();
		event_queue.pop_front();

		if (event.generation != p_generation || event.generation != runtime_generation || !event_handler) {
			continue;
		}

		auto target_entry = event_targets.find(event.tag);
		if (target_entry == event_targets.end()) {
			continue;
		}
		std::shared_ptr<RNEventTarget> target = target_entry->second.lock();
		if (!target || target->get_generation() != p_generation) {
			continue;
		}

		facebook::jsi::Value instance_handle = target->lock(p_runtime);
		if (!instance_handle.isObject()) {
			continue;
		}

		Dictionary payload = event.payload.duplicate(true);
		payload["target"] = event.tag;
		current_event_priority = event.priority;
		try {
			facebook::jsi::Value args[] = {
				facebook::jsi::Value(p_runtime, instance_handle),
				facebook::jsi::String::createFromUtf8(p_runtime, string_to_utf8(event.name)),
				variant_to_jsi(p_runtime, payload),
			};
			event_handler->call(p_runtime, static_cast<const facebook::jsi::Value *>(args), size_t(3));
		} catch (const facebook::jsi::JSIException &p_error) {
			WARN_PRINT(vformat("React event %s for tag %d failed: %s", event.name, event.tag, string_from_utf8(p_error.what())));
		}
		current_event_priority = EVENT_PRIORITY_DEFAULT;
	}

	if (!event_queue.empty()) {
		request_event_flush();
	}
}

void FabricUIManager::before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) {
	(void)p_runtime;
	(void)p_generation;
	event_handler.reset();
	for (auto &entry : event_targets) {
		if (std::shared_ptr<RNEventTarget> target = entry.second.lock()) {
			target->reset();
		}
	}
	event_targets.clear();
	clear_events();
	responder_tag = 0;
	current_event_priority = EVENT_PRIORITY_DEFAULT;
	++runtime_generation;
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
	if (name == "registerEventHandler") {
		return host_fn("registerEventHandler", 1, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return register_event_handler(rt_inner, args, argc);
		});
	}
	if (name == "setIsJSResponder") {
		return host_fn("setIsJSResponder", 3, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return set_is_js_responder(rt_inner, args, argc);
		});
	}
	if (name == "measure") {
		return host_fn("measure", 2, [this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value *args, size_t argc) {
			return measure(rt_inner, args, argc);
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
		return host_fn("unstable_getCurrentEventPriority", 0, [this](facebook::jsi::Runtime &, const facebook::jsi::Value *, size_t) {
			return facebook::jsi::Value(current_event_priority);
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
		"registerEventHandler",
		"setIsJSResponder",
		"measure",
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
