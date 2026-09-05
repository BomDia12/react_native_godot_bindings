#pragma once

#include "../runtime/react_native_runtime_coordinator.h"
#include "../singletons/hermes_runtime_lifecycle.h"

#include <jsi/jsi.h>

#include <memory>
#include <unordered_map>
#include <vector>

class FabricUIManager : public facebook::jsi::HostObject, public HermesRuntimeLifecycle, public std::enable_shared_from_this<FabricUIManager> {
	std::weak_ptr<RNRuntimeCoordinatorState> state;
	uint64_t runtime_generation = 0;
	std::unique_ptr<facebook::jsi::Function> event_handler;
	std::unordered_map<RNSurfaceTag, std::weak_ptr<class RNEventTarget>, RNSurfaceTagHash> event_targets;
	std::unordered_map<int, Ref<RNShadowNode>> virtual_roots;
	std::unordered_map<int, int> responder_tags;
	int current_event_priority = 0;

	facebook::jsi::Value create_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value clone_node(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc, bool new_children, bool new_props);
	facebook::jsi::Value create_child_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value append_child_to_set(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value complete_root(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value register_event_handler(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value set_is_js_responder(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value measure(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value measure_in_window(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value measure_layout(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value get_bounding_client_rect(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value set_native_props(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value dispatch_command(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	facebook::jsi::Value report_surface_error(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
	void dispatch_event_locked(facebook::jsi::Runtime &p_runtime, const RNNativeEvent &p_event, uint64_t p_generation);

public:
	static const char *GLOBAL_NAME;

	enum EventPriority {
		EVENT_PRIORITY_DEFAULT = 0,
		EVENT_PRIORITY_DISCRETE = 1,
		EVENT_PRIORITY_CONTINUOUS = 2,
		EVENT_PRIORITY_IDLE = 3,
	};

	explicit FabricUIManager(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state);

	facebook::jsi::Value get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &rt) override;

	void register_surface(const RNSurfaceRoute &p_route);
	void remove_surface(int p_root_tag, uint64_t p_epoch);
	facebook::jsi::Value link_root_node(facebook::jsi::Runtime &p_runtime, int p_root_tag, const facebook::jsi::Object &p_instance_handle);
	void dispatch_queued_events_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation);
	void before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) override;
	uint64_t get_runtime_generation() const { return runtime_generation; }
	int get_responder_tag(int p_root_tag) const;
};

class RNShadowNodeHandle : public facebook::jsi::HostObject {
public:
	Ref<RNShadowNode> node;

	explicit RNShadowNodeHandle(const Ref<RNShadowNode> &p_node) :
			node(p_node) {}
};

class RNChildSetHandle : public facebook::jsi::HostObject {
public:
	int root_tag = 0;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	Vector<Ref<RNShadowNode>> children;
};
