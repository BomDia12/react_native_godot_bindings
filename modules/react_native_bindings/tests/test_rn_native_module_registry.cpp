#include "../native_modules/rn_native_module_registry.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "tests/test_macros.h"

namespace TestRNNativeModuleRegistry {

class CompletedAsyncModule : public RNNativeModule {
public:
	RNModuleResult invoke_sync(const StringName &, const Array &, const RNCallContext &) override {
		return RNModuleResult::success(String("completed"));
	}
};

struct DeferredModuleState {
	RNCompletionToken completion;
	bool started = false;
};

class DeferredAsyncModule : public RNNativeModule {
	std::shared_ptr<DeferredModuleState> state;

public:
	explicit DeferredAsyncModule(const std::shared_ptr<DeferredModuleState> &p_state) :
			state(p_state) {
	}

	RNModuleResult invoke_sync(const StringName &, const Array &, const RNCallContext &) override {
		return RNModuleResult::success();
	}

	void start_async(const StringName &, const Array &, const RNCallContext &, const RNCompletionToken &p_completion) override {
		state->completion = p_completion;
		state->started = true;
	}
};

TEST_CASE("[ReactNativeBindings][NativeModules] cancellation replaces an undelivered completion") {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime != nullptr);
	runtime->reset();

	auto registry = std::make_shared<RNNativeModuleRegistry>(std::shared_ptr<RNRuntimeCoordinatorState>());
	registry->begin_generation(runtime->get_runtime_generation());
	RNMethodSchema work;
	work.name = "work";
	work.mode = RNCallMode::ASYNC;
	work.result = RNValueSchema::value(RNValueType::STRING);
	RNModuleDefinition definition;
	definition.name = "CompletedAsync";
	definition.methods.push_back(work);
	definition.factory = []() { return std::make_unique<CompletedAsyncModule>(); };
	RNError error;
	REQUIRE(registry->register_module(definition, error));
	runtime->install_host_object("__testNativeModules", registry);

	runtime->evaluate(
			"globalThis.cancelResult = 'pending';"
			"const module = __testNativeModules.get('CompletedAsync');"
			"const request = module.__godotStartAsync('work', []);"
			"globalThis.cancelRequest = request.requestId;"
			"request.promise.then("
			"  value => { globalThis.cancelResult = 'resolved:' + value; },"
			"  error => { globalThis.cancelResult = error.code; }"
			");"
			"undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	registry->process_jobs();
	runtime->evaluate("__testNativeModules.cancel(cancelRequest)");
	REQUIRE(runtime->get_last_error().is_empty());
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(String(runtime->get_global("cancelResult")) == RNErrorCode::CANCELLED);
	runtime->uninstall_host_object("__testNativeModules");
}

TEST_CASE("[ReactNativeBindings][NativeModules] deferred promises and subscriptions keep delivery pumping alive") {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime != nullptr);
	runtime->reset();

	auto state = std::make_shared<DeferredModuleState>();
	auto registry = std::make_shared<RNNativeModuleRegistry>(std::shared_ptr<RNRuntimeCoordinatorState>());
	registry->begin_generation(runtime->get_runtime_generation());
	RNMethodSchema work;
	work.name = "work";
	work.mode = RNCallMode::ASYNC;
	work.result = RNValueSchema::value(RNValueType::STRING);
	RNEventSchema changed;
	changed.name = "changed";
	changed.subscription_name = "onChanged";
	changed.payload = RNValueSchema::value(RNValueType::STRING);
	RNModuleDefinition definition;
	definition.name = "DeferredAsync";
	definition.methods.push_back(work);
	definition.events.push_back(changed);
	definition.factory = [state]() { return std::make_unique<DeferredAsyncModule>(state); };
	RNError error;
	REQUIRE(registry->register_module(definition, error));
	runtime->install_host_object("__testNativeModules", registry);

	runtime->evaluate(
			"globalThis.deferredResult = 'pending';"
			"globalThis.deferredModule = __testNativeModules.get('DeferredAsync');"
			"deferredModule.work().then(value => { globalThis.deferredResult = value; });"
			"undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	CHECK(registry->has_pending_work());
	registry->process_jobs();
	REQUIRE(state->started);
	CHECK(registry->has_pending_work());
	state->completion.complete(String("done"));
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(String(runtime->get_global("deferredResult")) == "done");
	CHECK_FALSE(registry->has_pending_work());

	runtime->evaluate("globalThis.deferredSubscription = deferredModule.onChanged(() => {}); undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	CHECK(registry->has_pending_work());
	runtime->evaluate("deferredSubscription.remove(); undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	CHECK_FALSE(registry->has_pending_work());
	runtime->uninstall_host_object("__testNativeModules");
}

} // namespace TestRNNativeModuleRegistry

void rn_force_link_native_module_registry_tests() {}
