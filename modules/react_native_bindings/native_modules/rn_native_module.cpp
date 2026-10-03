#include "rn_native_module.h"

#include "rn_native_module_registry.h"

RNCompletionToken::RNCompletionToken(const std::shared_ptr<RNNativeModuleRegistry> &p_registry, const String &p_request_token, const String &p_module_name, const String &p_session_token, uint64_t p_generation) :
		registry(p_registry),
		request_token(p_request_token),
		module_name(p_module_name),
		session_token(p_session_token),
		generation(p_generation) {
}

void RNCompletionToken::complete(const Variant &p_value) const {
	if (std::shared_ptr<RNNativeModuleRegistry> owner = registry.lock()) {
		owner->queue_completion(request_token, generation, p_value, RNError());
	}
}

void RNCompletionToken::fail(const RNError &p_error) const {
	if (std::shared_ptr<RNNativeModuleRegistry> owner = registry.lock()) {
		owner->queue_completion(request_token, generation, Variant(), p_error);
	}
}

bool RNCompletionToken::emit(const StringName &p_event, const Variant &p_payload) const {
	if (std::shared_ptr<RNNativeModuleRegistry> owner = registry.lock()) {
		return owner->queue_event(module_name, p_event, session_token, generation, p_payload);
	}
	return false;
}

void RNNativeModule::start_async(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context, const RNCompletionToken &p_completion) {
	const RNModuleResult result = invoke_sync(p_method, p_arguments, p_context);
	if (result.error.is_set()) {
		p_completion.fail(result.error);
	} else {
		p_completion.complete(result.value);
	}
}
