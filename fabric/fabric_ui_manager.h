#pragma once

#include "rn_shadow_node.h"

#include "core/object/object_id.h"

#include <jsi/jsi.h>

// global.nativeFabricUIManager: the one JSI object React Native's renderer talks to.
// Android, iOS and react-native-windows all plug into this same socket.
//
// Host functions here run inside HermesRuntimeSingleton::evaluate_locked(), with
// runtime_mutex held. They must never call back into the singleton's public API,
// and anything touching the scene tree goes out through call_deferred.
class FabricUIManager : public facebook::jsi::HostObject {
	ObjectID root_view_id;

	class ReactNativeRootView *get_root_view() const;

	facebook::jsi::Value create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value clone_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc, bool new_children, bool new_props);
	facebook::jsi::Value create_child_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child_to_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value complete_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);

public:
	static const char *GLOBAL_NAME;

	// Fabric's event priorities, mirroring ReactEventPriority in
	// ReactCommon/react/renderer/core/ReactEventPriority.h.
	enum EventPriority {
		EVENT_PRIORITY_DEFAULT = 0,
		EVENT_PRIORITY_DISCRETE = 1,
		EVENT_PRIORITY_CONTINUOUS = 2,
		EVENT_PRIORITY_IDLE = 3,
	};

	explicit FabricUIManager(ObjectID p_root_view_id);

	facebook::jsi::Value get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &rt) override;
};

// The opaque handles JS holds: a shadow node and a child set. React never looks
// inside them, it only hands them back to us.
class RNShadowNodeHandle : public facebook::jsi::HostObject {
public:
	Ref<RNShadowNode> node;

	explicit RNShadowNodeHandle(const Ref<RNShadowNode> &p_node) :
			node(p_node) {}
};

class RNChildSetHandle : public facebook::jsi::HostObject {
public:
	Vector<Ref<RNShadowNode>> children;
};
