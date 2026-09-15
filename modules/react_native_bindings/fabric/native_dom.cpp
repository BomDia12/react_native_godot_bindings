#include "native_dom.h"

#include "fabric_ui_manager.h"
#include "rn_event_target.h"

#include "core/error/error_macros.h"

#include <cmath>
#include <string>
#include <vector>

namespace {
std::string to_utf8(const String &p_value) {
	const CharString value = p_value.utf8();
	return std::string(value.get_data(), value.length());
}

String from_utf8(const std::string &p_value) {
	return String::utf8(p_value.c_str(), int(p_value.length()));
}

Ref<RNShadowNode> node_from(facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &p_value) {
	if (!p_value.isObject()) {
		return Ref<RNShadowNode>();
	}
	facebook::jsi::Object object = p_value.getObject(p_runtime);
	return object.isHostObject<RNShadowNodeHandle>(p_runtime) ? object.getHostObject<RNShadowNodeHandle>(p_runtime)->node : Ref<RNShadowNode>();
}

struct ResolvedNode {
	int root_tag = 0;
	int tag = 0;
	bool document = false;
	std::shared_ptr<const RNSurfaceSnapshot> snapshot;
	const RNMountedNodeSnapshot *node = nullptr;
};

ResolvedNode resolve(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state, facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &p_value) {
	ResolvedNode result;
	if (!p_state) {
		return result;
	}
	if (p_value.isNumber()) {
		result.root_tag = int(p_value.getNumber());
		result.tag = result.root_tag;
		result.document = true;
	} else {
		Ref<RNShadowNode> node = node_from(p_runtime, p_value);
		if (node.is_null()) {
			return result;
		}
		result.root_tag = node->root_tag;
		result.tag = node->tag;
		auto route = p_state->routes.find(result.root_tag);
		if (route == p_state->routes.end() || route->second.runtime_generation != node->runtime_generation || route->second.surface_epoch != node->surface_epoch) {
			return ResolvedNode();
		}
	}
	auto snapshot = p_state->snapshots.find(result.root_tag);
	if (snapshot == p_state->snapshots.end() || !snapshot->second) {
		return ResolvedNode();
	}
	result.snapshot = snapshot->second;
	result.node = result.snapshot->nodes.getptr(result.tag);
	if (!result.document && !result.node) {
		return ResolvedNode();
	}
	return result;
}

facebook::jsi::Value instance_handle(facebook::jsi::Runtime &p_runtime, const RNMountedNodeSnapshot *p_node) {
	if (!p_node || p_node->shadow_node.is_null() || !p_node->shadow_node->event_target) {
		return facebook::jsi::Value::undefined();
	}
	return p_node->shadow_node->event_target->lock(p_runtime);
}

facebook::jsi::Array number_array(facebook::jsi::Runtime &p_runtime, std::initializer_list<double> p_values) {
	facebook::jsi::Array result(p_runtime, p_values.size());
	size_t index = 0;
	for (double value : p_values) {
		result.setValueAtIndex(p_runtime, index++, value);
	}
	return result;
}

bool is_ancestor(const RNSurfaceSnapshot &p_snapshot, int p_ancestor, int p_node) {
	for (int current = p_node; current != 0;) {
		if (current == p_ancestor) {
			return true;
		}
		const RNMountedNodeSnapshot *node = p_snapshot.nodes.getptr(current);
		current = node ? node->parent_tag : 0;
	}
	return false;
}

void document_order(const RNSurfaceSnapshot &p_snapshot, int p_tag, Vector<int> &r_order) {
	r_order.push_back(p_tag);
	const RNMountedNodeSnapshot *node = p_snapshot.nodes.getptr(p_tag);
	if (!node) {
		return;
	}
	for (int child : node->child_tags) {
		document_order(p_snapshot, child, r_order);
	}
}

int order_index(const Vector<int> &p_order, int p_tag) {
	for (int i = 0; i < p_order.size(); ++i) {
		if (p_order[i] == p_tag) {
			return i;
		}
	}
	return -1;
}

} // namespace

facebook::jsi::Value NativeDOM::get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(rt);
	auto host_fn = [&](unsigned int p_count, auto p_body) {
		return facebook::jsi::Function::createFromHostFunction(rt, facebook::jsi::PropNameID::forUtf8(rt, name), p_count, [p_body](facebook::jsi::Runtime &inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) { return p_body(inner, args, argc); });
	};
	if (name == "linkRootNode") {
		return host_fn(2, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			if (argc < 2 || !args[0].isNumber() || !args[1].isObject()) {
				return facebook::jsi::Value::undefined();
			}
			auto shared = state.lock();
			auto manager = shared ? shared->ui_manager.lock() : nullptr;
			return manager ? manager->link_root_node(inner, int(args[0].getNumber()), args[1].getObject(inner)) : facebook::jsi::Value::undefined();
		});
	}
	if (name == "compareDocumentPosition") {
		return host_fn(2, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			if (argc < 2) {
				return facebook::jsi::Value(1);
			}
			auto shared = state.lock();
			ResolvedNode left = resolve(shared, inner, args[0]);
			ResolvedNode right = resolve(shared, inner, args[1]);
			if (!left.snapshot || !right.snapshot || left.root_tag != right.root_tag) {
				return facebook::jsi::Value(1);
			}
			if (left.tag == right.tag) {
				return facebook::jsi::Value(0);
			}
			if (left.document && right.document) {
				return facebook::jsi::Value(0);
			}
			if (left.document) {
				return facebook::jsi::Value(20);
			}
			if (right.document) {
				return facebook::jsi::Value(10);
			}
			if (is_ancestor(*left.snapshot, left.tag, right.tag)) {
				return facebook::jsi::Value(20);
			}
			if (is_ancestor(*left.snapshot, right.tag, left.tag)) {
				return facebook::jsi::Value(10);
			}
			Vector<int> order;
			document_order(*left.snapshot, left.root_tag, order);
			return facebook::jsi::Value(order_index(order, left.tag) < order_index(order, right.tag) ? 4 : 2);
		});
	}
	if (name == "getChildNodes") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			auto shared = state.lock();
			ResolvedNode value = argc ? resolve(shared, inner, args[0]) : ResolvedNode();
			if (!value.snapshot) {
				return facebook::jsi::Array(inner, 0);
			}
			Vector<int> children;
			if (value.document) {
				children.push_back(value.root_tag);
			} else if (value.node) {
				children = value.node->child_tags;
			}
			std::vector<facebook::jsi::Value> handles;
			for (int tag : children) {
				facebook::jsi::Value handle = instance_handle(inner, value.snapshot->nodes.getptr(tag));
				if (handle.isObject()) {
					handles.push_back(std::move(handle));
				}
			}
			facebook::jsi::Array result(inner, handles.size());
			for (size_t i = 0; i < handles.size(); ++i) {
				result.setValueAtIndex(inner, i, std::move(handles[i]));
			}
			return result;
		});
	}
	if (name == "getParentNode") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			if (!value.snapshot || value.document || !value.node) {
				return facebook::jsi::Value::undefined();
			}
			if (value.tag == value.root_tag) {
				return facebook::jsi::Value(value.root_tag);
			}
			return instance_handle(inner, value.snapshot->nodes.getptr(value.node->parent_tag));
		});
	}
	if (name == "getElementById") {
		return host_fn(2, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			if (argc < 2 || !args[0].isNumber() || !args[1].isString()) {
				return facebook::jsi::Value::undefined();
			}
			ResolvedNode document = resolve(state.lock(), inner, args[0]);
			if (!document.snapshot) {
				return facebook::jsi::Value::undefined();
			}
			const String id = from_utf8(args[1].getString(inner).utf8(inner));
			const Vector<int> *matches = document.snapshot->native_id_index.getptr(id);
			return matches && !matches->is_empty() ? instance_handle(inner, document.snapshot->nodes.getptr((*matches)[0])) : facebook::jsi::Value::undefined();
		});
	}
	if (name == "isConnected") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) { return facebook::jsi::Value(argc > 0 && resolve(state.lock(), inner, args[0]).snapshot != nullptr); });
	}
	if (name == "getTagName") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			return facebook::jsi::String::createFromUtf8(inner, to_utf8(value.node ? "RN:" + value.node->view_name : String()));
		});
	}
	if (name == "getTextContent") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			return facebook::jsi::String::createFromUtf8(inner, to_utf8(value.node ? value.node->text_content : String()));
		});
	}
	if (name == "getBorderWidth") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			Vector4 border = value.node ? value.node->border_width : Vector4();
			return number_array(inner, { std::round(border.x), std::round(border.y), std::round(border.z), std::round(border.w) });
		});
	}
	if (name == "getBoundingClientRect") {
		return host_fn(2, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			Rect2 rect = value.node ? value.node->window_rect : Rect2();
			return number_array(inner, { rect.position.x, rect.position.y, rect.size.x, rect.size.y });
		});
	}
	if (name == "getInnerSize") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			Size2 size = value.node ? value.node->inner_size : Size2();
			return number_array(inner, { std::round(size.x), std::round(size.y) });
		});
	}
	if (name == "getOffset") {
		return host_fn(1, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			ResolvedNode value = argc ? resolve(state.lock(), inner, args[0]) : ResolvedNode();
			facebook::jsi::Array result(inner, 3);
			if (!value.node) {
				result.setValueAtIndex(inner, 0, facebook::jsi::Value::null());
				result.setValueAtIndex(inner, 1, 0);
				result.setValueAtIndex(inner, 2, 0);
				return result;
			}
			facebook::jsi::Value parent = instance_handle(inner, value.snapshot->nodes.getptr(value.node->offset_parent_tag));
			result.setValueAtIndex(inner, 0, parent.isObject() ? std::move(parent) : facebook::jsi::Value::null());
			result.setValueAtIndex(inner, 1, value.node->offset.y);
			result.setValueAtIndex(inner, 2, value.node->offset.x);
			return result;
		});
	}
	if (name == "measure" || name == "measureInWindow" || name == "measureLayout") {
		return host_fn(name == "measureLayout" ? 4 : 2, [this, name](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			auto shared = state.lock();
			auto manager = shared ? shared->ui_manager.lock() : nullptr;
			if (!manager) {
				return facebook::jsi::Value::undefined();
			}
			facebook::jsi::PropNameID property = facebook::jsi::PropNameID::forUtf8(inner, name);
			facebook::jsi::Value function = manager->get(inner, property);
			return function.getObject(inner).getFunction(inner).call(inner, args, argc);
		});
	}
	if (name == "setNativeProps") {
		return host_fn(2, [this](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			auto shared = state.lock();
			auto manager = shared ? shared->ui_manager.lock() : nullptr;
			if (!manager) {
				return facebook::jsi::Value::undefined();
			}
			facebook::jsi::PropNameID property = facebook::jsi::PropNameID::forAscii(inner, "setNativeProps");
			facebook::jsi::Value function = manager->get(inner, property);
			return function.getObject(inner).getFunction(inner).call(inner, args, argc);
		});
	}
	if (name == "hasPointerCapture" || name == "setPointerCapture" || name == "releasePointerCapture") {
		return host_fn(2, [this, name](facebook::jsi::Runtime &inner, const facebook::jsi::Value *args, size_t argc) {
			Ref<RNShadowNode> node = argc ? node_from(inner, args[0]) : Ref<RNShadowNode>();
			if (node.is_null() || argc < 2 || !args[1].isNumber()) {
				return facebook::jsi::Value(false);
			}
			auto shared = state.lock();
			if (!shared) {
				return facebook::jsi::Value(false);
			}
			const int pointer_id = int(args[1].getNumber());
			if (name == "hasPointerCapture") {
				return facebook::jsi::Value(shared->pointer_capture.has_capture(node->root_tag, node->surface_epoch, node->tag, pointer_id));
			}
			if (name == "setPointerCapture") {
				shared->pointer_capture.set_capture(node->root_tag, node->surface_epoch, node->tag, pointer_id);
			} else {
				shared->pointer_capture.release_capture(node->root_tag, node->surface_epoch, node->tag, pointer_id);
			}
			return facebook::jsi::Value::undefined();
		});
	}
	if (name == "getScrollPosition" || name == "getScrollSize") {
		return host_fn(1, [name](facebook::jsi::Runtime &inner, const facebook::jsi::Value *, size_t) {
			WARN_PRINT_ONCE(String("NativeDOM.") + from_utf8(name) + "() is unsupported until ScrollView state is implemented.");
			return number_array(inner, { 0, 0 });
		});
	}
	return facebook::jsi::Value::undefined();
}

std::vector<facebook::jsi::PropNameID> NativeDOM::getPropertyNames(facebook::jsi::Runtime &rt) {
	static const char *NAMES[] = { "linkRootNode", "compareDocumentPosition", "getChildNodes", "getParentNode", "getElementById", "isConnected", "getTagName", "getTextContent", "getBorderWidth", "getBoundingClientRect", "getInnerSize", "getOffset", "measure", "measureInWindow", "measureLayout", "setNativeProps", "hasPointerCapture", "setPointerCapture", "releasePointerCapture", "getScrollPosition", "getScrollSize" };
	std::vector<facebook::jsi::PropNameID> result;
	for (const char *name : NAMES) {
		result.push_back(facebook::jsi::PropNameID::forAscii(rt, name));
	}
	return result;
}
