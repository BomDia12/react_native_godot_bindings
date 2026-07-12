#include "rn_scratch_host.h"

#include "../root_view/react_native_root_view.h"

#include "core/error/error_macros.h"
#include "core/object/object.h"
#include "core/variant/array.h"

#include <string>

const char *RNScratchHost::GLOBAL_NAME = "__godotRNHost";

const char *RNScratchHost::JS_SHIM =
		"globalThis.__godotRNCreateNode = (type, props) => __godotRNHost.createNode(type, props || {});\n"
		"globalThis.__godotRNCreateRawText = (text) => __godotRNHost.createRawText(String(text));\n"
		"globalThis.__godotRNAppendChild = (parent, child) => __godotRNHost.appendChild(parent, child);\n"
		"globalThis.__godotRNCommitRoot = (tag) => __godotRNHost.commitRoot(tag);\n";

namespace {
	String string_from_utf8(const std::string &p_value) {
		return String::utf8(p_value.c_str());
	}

	// Shallow props conversion. Enough for the scratch gate; the real one lands in Phase 3.
	Dictionary props_to_dictionary(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
		Dictionary result;
		if (!p_value.isObject()) {
			return result;
		}

		facebook::jsi::Object object = p_value.getObject(rt);
		facebook::jsi::Array names = object.getPropertyNames(rt);
		for (size_t i = 0; i < names.size(rt); ++i) {
			facebook::jsi::Value key = names.getValueAtIndex(rt, i);
			if (!key.isString()) {
				continue;
			}

			String key_string = string_from_utf8(key.getString(rt).utf8(rt));
			facebook::jsi::Value value = object.getProperty(rt, key.getString(rt));
			if (value.isBool()) {
				result[key_string] = value.getBool();
			} else if (value.isNumber()) {
				result[key_string] = value.getNumber();
			} else if (value.isString()) {
				result[key_string] = string_from_utf8(value.getString(rt).utf8(rt));
			}
		}
		return result;
	}
} //namespace

RNScratchHost::RNScratchHost(ObjectID p_root_view_id) : root_view_id(p_root_view_id) {
}

void RNScratchHost::reset_state() {
	nodes.clear();
}

ReactNativeRootView *RNScratchHost::get_root_view() const {
	return Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(root_view_id));
}

Dictionary RNScratchHost::build_tree(int p_tag) const {
	Dictionary result;
	const ScratchNode *node = nodes.getptr(p_tag);
	if (!node) {
		return result;
	}

	result["tag"] = p_tag;
	result["viewName"] = node->view_name;
	result["text"] = node->text;
	result["props"] = node->props;

	Array children;
	for (int child_tag : node->children) {
		children.push_back(build_tree(child_tag));
	}
	result["children"] = children;
	return result;
}

facebook::jsi::Value RNScratchHost::create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 1 || !p_args[0].isString()) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCreateNode expects (viewName, props)."));
	}

	ReactNativeRootView *root_view = get_root_view();
	if (!root_view) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCreateNode: root view is gone."));
	}

	ScratchNode node;
	node.view_name = string_from_utf8(p_args[0].getString(rt).utf8(rt));
	if (p_argc >= 2) {
		node.props = props_to_dictionary(rt, p_args[1]);
	}

	const int tag = root_view->get_registry().allocate_tag();
	nodes[tag] = node;
	return facebook::jsi::Value(tag);
}

facebook::jsi::Value RNScratchHost::create_raw_text(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 1 || !p_args[0].isString()) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCreateRawText expects (text)."));
	}

	ReactNativeRootView *root_view = get_root_view();
	if (!root_view) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCreateRawText: root view is gone."));
	}

	ScratchNode node;
	node.view_name = "RCTRawText";
	node.text = string_from_utf8(p_args[0].getString(rt).utf8(rt));

	const int tag = root_view->get_registry().allocate_tag();
	nodes[tag] = node;
	return facebook::jsi::Value(tag);
}

facebook::jsi::Value RNScratchHost::append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 2 || !p_args[0].isNumber() || !p_args[1].isNumber()) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNAppendChild expects (parentTag, childTag)."));
	}

	const int parent_tag = static_cast<int>(p_args[0].getNumber());
	const int child_tag = static_cast<int>(p_args[1].getNumber());

	ScratchNode *parent = nodes.getptr(parent_tag);
	if (!parent || !nodes.has(child_tag)) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNAppendChild: unknown tag."));
	}

	parent->children.push_back(child_tag);
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value RNScratchHost::commit_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (p_argc < 1 || !p_args[0].isNumber()) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCommitRoot expects (tag)."));
	}

	const int tag = static_cast<int>(p_args[0].getNumber());
	if (!nodes.has(tag)) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCommitRoot: unknown tag."));
	}

	ReactNativeRootView *root_view = get_root_view();
	if (!root_view) {
		throw facebook::jsi::JSError(rt, std::string("__godotRNCommitRoot: root view is gone."));
	}

	// Deferred on purpose: we are inside evaluate_locked() with runtime_mutex held.
	// Touching the scene tree from here and re-entering the runtime would deadlock.
	root_view->call_deferred("mount_scratch_tree", build_tree(tag));
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value RNScratchHost::get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(rt);

	if (name == "createNode") {
		return facebook::jsi::Function::createFromHostFunction(
				rt, facebook::jsi::PropNameID::forAscii(rt, "createNode"), 2,
				[this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) {
					return create_node(rt_inner, args, argc);
				});
	}

	if (name == "createRawText") {
		return facebook::jsi::Function::createFromHostFunction(
				rt, facebook::jsi::PropNameID::forAscii(rt, "createRawText"), 1,
				[this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) {
					return create_raw_text(rt_inner, args, argc);
				});
	}

	if (name == "appendChild") {
		return facebook::jsi::Function::createFromHostFunction(
				rt, facebook::jsi::PropNameID::forAscii(rt, "appendChild"), 2,
				[this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) {
					return append_child(rt_inner, args, argc);
				});
	}

	if (name == "commitRoot") {
		return facebook::jsi::Function::createFromHostFunction(
				rt, facebook::jsi::PropNameID::forAscii(rt, "commitRoot"), 1,
				[this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) {
					return commit_root(rt_inner, args, argc);
				});
	}

	return facebook::jsi::Value::undefined();
}

std::vector<facebook::jsi::PropNameID> RNScratchHost::getPropertyNames(facebook::jsi::Runtime &rt) {
	std::vector<facebook::jsi::PropNameID> names;
	names.push_back(facebook::jsi::PropNameID::forAscii(rt, "createNode"));
	names.push_back(facebook::jsi::PropNameID::forAscii(rt, "createRawText"));
	names.push_back(facebook::jsi::PropNameID::forAscii(rt, "appendChild"));
	names.push_back(facebook::jsi::PropNameID::forAscii(rt, "commitRoot"));
	return names;
}
