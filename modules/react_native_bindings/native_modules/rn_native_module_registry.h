#pragma once

#include "../runtime/rn_execution_scope.h"
#include "../singletons/hermes_runtime_lifecycle.h"
#include "rn_native_module.h"

#include "core/templates/hash_set.h"

#include <jsi/jsi.h>

#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

struct RNRuntimeCoordinatorState;

class RNNativeModuleRegistry : public facebook::jsi::HostObject, public HermesRuntimeLifecycle, public std::enable_shared_from_this<RNNativeModuleRegistry> {
	struct NativeJob {
		String request_token;
		String module_name;
		StringName method;
		Array arguments;
		RNCallContext context;
	};
	struct NativeCompletion {
		uint64_t bytes = 0;
		uint64_t order = 0;
		String request_token;
		uint64_t generation = 0;
		Variant value;
		RNError error;
	};
	struct NativeEvent {
		uint64_t order = 0;
		String module_name;
		StringName event;
		String session_token;
		uint64_t generation = 0;
		Variant payload;
		std::vector<std::string> listeners;
		size_t next_listener = 0;
		bool listeners_captured = false;
	};
	struct PendingPromise {
		String module_name;
		StringName method;
		String session_token;
		RNValueSchema result_schema;
		std::unique_ptr<facebook::jsi::Function> resolve;
		std::unique_ptr<facebook::jsi::Function> reject;
		bool settled = false;
		RNError cancellation_error;
	};
	struct Subscription {
		bool keeps_runtime_alive = true;
		RNExecutionOrigin origin;
		String module_name;
		StringName event;
		String session_token;
		uint64_t generation = 0;
		std::unique_ptr<facebook::jsi::Function> callback;
	};

	std::weak_ptr<RNRuntimeCoordinatorState> state;
	RNObjectRegistry objects;
	HashMap<String, RNModuleDefinition> definitions;
	HashMap<String, std::shared_ptr<RNNativeModule>> instances;
	std::unordered_map<std::string, std::unique_ptr<facebook::jsi::Object>> module_cache;
	std::unordered_map<std::string, PendingPromise> pending_promises;
	std::unordered_map<std::string, Subscription> subscriptions;
	std::deque<NativeJob> jobs;
	std::deque<NativeCompletion> completions;
	std::deque<NativeEvent> events;
	uint64_t event_bytes = 0;
	uint64_t completion_bytes = 0;
	HashSet<String> queued_completions;
	uint64_t next_delivery = 1;
	HashSet<String> cancelled_requests;
	mutable std::mutex delivery_mutex;
	uint64_t generation = 0;
	uint64_t next_request = 1;
	uint64_t next_subscription = 1;
	bool definitions_frozen = false;
	bool accepting_work = true;

	const RNModuleDefinition *definition(const String &p_name) const;
	const RNMethodSchema *method_schema(const RNModuleDefinition &p_definition, const StringName &p_method) const;
	const RNEventSchema *event_schema(const RNModuleDefinition &p_definition, const StringName &p_subscription) const;
	Dictionary module_metadata(const RNModuleDefinition &p_definition) const;
	std::shared_ptr<RNNativeModule> instance(const RNModuleDefinition &p_definition);
	bool decode_arguments(facebook::jsi::Runtime &p_runtime, const String &p_module, const RNMethodSchema &p_schema, const facebook::jsi::Value *p_arguments, size_t p_count, Array &r_arguments, RNCallContext &r_context, RNError &r_error);
	facebook::jsi::Value module_for_locked(facebook::jsi::Runtime &p_runtime, const String &p_name);
	facebook::jsi::Value invoke_locked(facebook::jsi::Runtime &p_runtime, const String &p_module, const RNMethodSchema &p_schema, const facebook::jsi::Value *p_arguments, size_t p_count, bool p_include_request);
	facebook::jsi::Value subscribe_locked(facebook::jsi::Runtime &p_runtime, const String &p_module, const RNEventSchema &p_schema, const facebook::jsi::Value *p_arguments, size_t p_count);
	facebook::jsi::Value make_error(facebook::jsi::Runtime &p_runtime, const RNError &p_error) const;
	void reject_promise_locked(facebook::jsi::Runtime &p_runtime, PendingPromise &p_pending, const RNError &p_error);
	void remove_subscription_locked(const std::string &p_token);

public:
	explicit RNNativeModuleRegistry(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state);

	bool register_module(const RNModuleDefinition &p_definition, RNError &r_error);
	void freeze_definitions() { definitions_frozen = true; }
	void begin_generation(uint64_t p_generation);
	void close_surface(int p_root_tag, uint64_t p_epoch);
	void process_jobs();
	void process_frame(double p_now_ms);
	void scene_binding_changed(ObjectID p_root);
	size_t deliver_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation, const std::function<void()> &p_checkpoint = {});
	bool has_pending_work() const;
	RNObjectRegistry &get_objects() { return objects; }
	Dictionary get_module_metadata(const String &p_name) const;

	void queue_completion(const String &p_request_token, uint64_t p_generation, const Variant &p_value, const RNError &p_error);
	bool can_queue_event(uint64_t p_bytes) const;
	bool queue_event(const String &p_module, const StringName &p_event, const String &p_session, uint64_t p_generation, const Variant &p_payload);
	void cancel_request(const String &p_request_token, const RNError &p_error);

	facebook::jsi::Value get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &p_runtime) override;
	void before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) override;

	facebook::jsi::Value get_module_member(facebook::jsi::Runtime &p_runtime, const String &p_module, const std::string &p_member);
};

class RNModuleProxy : public facebook::jsi::HostObject {
	std::weak_ptr<RNNativeModuleRegistry> registry;
	String module_name;

public:
	RNModuleProxy(const std::shared_ptr<RNNativeModuleRegistry> &p_registry, const String &p_module_name) :
			registry(p_registry),
			module_name(p_module_name) {}

	facebook::jsi::Value get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &p_runtime) override;
};
