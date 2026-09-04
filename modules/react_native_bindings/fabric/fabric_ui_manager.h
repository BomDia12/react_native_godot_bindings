#pragma once

#include "../singletons/hermes_runtime_lifecycle.h"
#include "rn_native_event.h"
#include "rn_shadow_node.h"

#include "core/object/object_id.h"

#include <jsi/jsi.h>

#include <deque>
#include <memory>
#include <unordered_map>

// global.nativeFabricUIManager: the one JSI object React Native's renderer talks to.
// Android, iOS and react-native-windows all plug into this same socket.
//
// Host functions here run inside HermesRuntimeSingleton::evaluate_locked(), with
// runtime_mutex held. They must never call back into the singleton's public API,
// and anything touching the scene tree goes out through call_deferred.
class FabricUIManager : public facebook::jsi::HostObject, public HermesRuntimeLifecycle {
	ObjectID root_view_id;
	uint64_t runtime_generation;
	std::unique_ptr<facebook::jsi::Function> event_handler;
	std::unordered_map<int, std::weak_ptr<class RNEventTarget>> event_targets;
	std::deque<RNNativeEvent> event_queue;
	int current_event_priority = EVENT_PRIORITY_DEFAULT;
	int responder_tag = 0;
	bool event_flush_scheduled = false;

	class ReactNativeRootView *get_root_view() const;

	facebook::jsi::Value create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value clone_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc, bool new_children, bool new_props);
	facebook::jsi::Value create_child_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child_to_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value complete_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value register_event_handler(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value set_is_js_responder(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value measure(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);

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

	FabricUIManager(ObjectID p_root_view_id, uint64_t p_runtime_generation);

	facebook::jsi::Value get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &rt) override;

	void enqueue_event(const RNNativeEvent &p_event);
	void request_event_flush();
	void clear_events();
	void dispatch_queued_events_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation);
	void before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) override;
	uint64_t get_runtime_generation() const { return runtime_generation; }
	int get_responder_tag() const { return responder_tag; }
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
