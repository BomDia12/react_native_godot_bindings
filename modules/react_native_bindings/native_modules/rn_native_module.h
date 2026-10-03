#pragma once

#include "../interop/rn_object_registry.h"
#include "../interop/rn_schema.h"

#include <functional>
#include <memory>

class RNNativeModuleRegistry;

struct RNCallContext {
	uint64_t generation = 0;
	std::weak_ptr<RNNativeModuleRegistry> registry;
	String request_token;
	String session_token;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
	RNObjectRegistry *objects = nullptr;
};

struct RNModuleResult {
	Variant value;
	RNError error;

	static RNModuleResult success(const Variant &p_value = Variant()) {
		RNModuleResult result;
		result.value = p_value;
		return result;
	}
	static RNModuleResult failure(const RNError &p_error) {
		RNModuleResult result;
		result.error = p_error;
		return result;
	}
};

class RNCompletionToken {
	std::weak_ptr<RNNativeModuleRegistry> registry;
	String request_token;
	String module_name;
	String session_token;
	uint64_t generation = 0;

public:
	RNCompletionToken() = default;
	RNCompletionToken(const std::shared_ptr<RNNativeModuleRegistry> &p_registry, const String &p_request_token, const String &p_module_name, const String &p_session_token, uint64_t p_generation);

	void complete(const Variant &p_value) const;
	void fail(const RNError &p_error) const;
	bool emit(const StringName &p_event, const Variant &p_payload) const;
};

class RNNativeModule {
public:
	virtual ~RNNativeModule() = default;
	virtual RNModuleResult invoke_sync(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context) = 0;
	virtual void start_async(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context, const RNCompletionToken &p_completion);
	virtual void process_frame(double p_now_ms) { (void)p_now_ms; }
	virtual void on_session_closed(const String &p_session) { (void)p_session; }
	virtual void on_surface_closed(int p_root_tag, uint64_t p_epoch) {
		(void)p_root_tag;
		(void)p_epoch;
	}
	virtual void on_scene_binding_changed(ObjectID p_root) { (void)p_root; }
	virtual void on_result_delivered(const String &p_request, bool p_success) {
		(void)p_request;
		(void)p_success;
	}
	virtual void shutdown() {}
	virtual bool has_pending_work() const { return false; }
	virtual void cancel(const String &p_request_token) { (void)p_request_token; }
};

struct RNModuleDefinition {
	String name;
	Vector<RNMethodSchema> methods;
	Vector<RNEventSchema> events;
	std::function<std::unique_ptr<RNNativeModule>()> factory;
};
