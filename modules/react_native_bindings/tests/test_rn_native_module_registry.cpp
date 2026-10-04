#include "../native_modules/rn_native_module_registry.h"
#include "../root_view/react_native_root_view.h"
#include "../runtime/react_native_runtime_coordinator.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

namespace TestRNNativeModuleRegistry {

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator][SceneTree] idle frame signal disconnects after draining and reconnects on root entry") {
	auto coordinator = ReactNativeRuntimeCoordinator::get_singleton();
	auto runtime = HermesRuntimeSingleton::get_singleton();
	auto tree = SceneTree::get_singleton();
	REQUIRE(coordinator);
	REQUIRE(runtime);
	REQUIRE(tree);
	runtime->reset();
	coordinator->get_native_module_registry()->begin_generation(runtime->get_runtime_generation());
	auto state = coordinator->get_state();
	state->bundle_generation = runtime->get_runtime_generation();
	state->bundle_status = RNBundleStatus::FAILED;
	state->bundle_error = "Fixture has no application bundle.";
	runtime->evaluate("globalThis.lifecycleService=__godotNativeModules.get('GodotServices');globalThis.lifecycleSubscription=lifecycleService.onEvent(()=>{});lifecycleService.getState();undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	const Callable frame = callable_mp(coordinator, &ReactNativeRuntimeCoordinator::_process_frame);
	for (int turn = 0; turn < 2; ++turn) {
		auto root = memnew(ReactNativeRootView);
		ERR_PRINT_OFF;
		tree->get_root()->add_child(root);
		MessageQueue::get_singleton()->flush();
		ERR_PRINT_ON;
		CHECK(tree->is_connected("process_frame", frame));
		runtime->evaluate("globalThis.drained=false;__godotScheduler.setTimeout(()=>{drained=true;},0);undefined;");
		ERR_PRINT_OFF;
		tree->get_root()->remove_child(root);
		ERR_PRINT_ON;
		memdelete(root);
		tree->notification(Node::NOTIFICATION_APPLICATION_PAUSED);
		CHECK(state->application_paused);
		CHECK(runtime->evaluate("lifecycleService.getState().initialAppState") == Variant("background"));
		tree->notification(Node::NOTIFICATION_APPLICATION_RESUMED);
		CHECK_FALSE(state->application_paused);
		CHECK(runtime->evaluate("lifecycleService.getState().initialAppState") == Variant("active"));
		CHECK(tree->is_connected("process_frame", frame));
		coordinator->_process_frame();
		CHECK(runtime->get_global("drained") == Variant(true));
		tree->notification(Node::NOTIFICATION_APPLICATION_PAUSED);
		CHECK(state->application_paused);
		CHECK(runtime->evaluate("lifecycleService.getState().initialAppState") == Variant("background"));
		tree->notification(Node::NOTIFICATION_APPLICATION_RESUMED);
		CHECK_FALSE(state->application_paused);
		CHECK(runtime->evaluate("lifecycleService.getState().initialAppState") == Variant("active"));
		CHECK(tree->is_connected("process_frame", frame));
		coordinator->_process_frame();
		CHECK_FALSE(tree->is_connected("process_frame", frame));
	}
	runtime->evaluate("lifecycleSubscription.remove();undefined;");
	state->bundle_status = RNBundleStatus::UNEVALUATED;
	state->bundle_generation = 0;
	state->bundle_error = String();
	runtime->reset();
}

class CompletedAsyncModule : public RNNativeModule {
public:
	RNModuleResult invoke_sync(const StringName &, const Array &, const RNCallContext &) override {
		return RNModuleResult::success(String("completed"));
	}
};

class LargeCompletionModule : public RNNativeModule {
	int sequence = 0;

public:
	RNModuleResult invoke_sync(const StringName &, const Array &, const RNCallContext &) override {
		PackedByteArray value;
		value.resize(4 * 1024 * 1024);
		value.set(0, ++sequence);
		return RNModuleResult::success(value);
	}
};

TEST_CASE("[ReactNativeBindings][NativeModules] aggregate completion bytes reject overflow and recover after cancellation delivery and reset") {
	auto runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime);
	runtime->reset();
	auto registry = std::make_shared<RNNativeModuleRegistry>(std::shared_ptr<RNRuntimeCoordinatorState>());
	registry->begin_generation(runtime->get_runtime_generation());
	RNMethodSchema work;
	work.name = "work";
	work.mode = RNCallMode::ASYNC;
	work.result = RNValueSchema::value(RNValueType::BYTES);
	RNModuleDefinition definition;
	definition.name = "LargeCompletion";
	definition.methods.push_back(work);
	definition.factory = [] { return std::make_unique<LargeCompletionModule>(); };
	RNError error;
	REQUIRE(registry->register_module(definition, error));
	runtime->install_host_object("__testNativeModules", registry);
	runtime->evaluate("globalThis.large=__testNativeModules.get('LargeCompletion');globalThis.results=[];globalThis.requests=[];globalThis.run=()=>{const op=large.__godotStartAsync('work',[]);requests.push(op.requestId);op.promise.then(value=>results.push(value[0]),error=>results.push(error.code));};for(let i=0;i<5;i++)run();undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	registry->process_jobs();
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 3 * (4 * 1024 * 1024 + 64));
	runtime->evaluate("__testNativeModules.cancel(requests[0]);run();undefined;");
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 2 * (4 * 1024 * 1024 + 64));
	registry->process_jobs();
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 3 * (4 * 1024 * 1024 + 64));
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(runtime->evaluate("JSON.stringify(results)") == Variant("[2,3,\"E_LIMIT\",\"E_LIMIT\",\"E_CANCELLED\",6]"));
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 0);
	CHECK_FALSE(registry->has_pending_work());
	runtime->evaluate("run();undefined;");
	registry->process_jobs();
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 4 * 1024 * 1024 + 64);
	const String late_request = runtime->evaluate("requests[requests.length-1]");
	const uint64_t old_generation = runtime->get_runtime_generation();
	runtime->reset();
	CHECK_FALSE(registry->has_pending_work());
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 0);
	PackedByteArray late_value;
	late_value.resize(1024);
	registry->queue_completion(late_request, old_generation, late_value, RNError());
	CHECK_FALSE(registry->has_pending_work());
	CHECK(int64_t(runtime->evaluate("__testNativeModules.getStats().completionBytes")) == 0);
	runtime->uninstall_host_object("__testNativeModules");
}

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

TEST_CASE("[ReactNativeBindings][NativeModules] event callbacks can change subscriptions during delivery") {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime != nullptr);
	runtime->reset();
	const uint64_t generation = runtime->get_runtime_generation();
	auto state = std::make_shared<RNRuntimeCoordinatorState>();
	RNSurfaceRoute route;
	route.root_tag = 11;
	route.runtime_generation = generation;
	route.surface_epoch = 1;
	route.status = RNSurfaceStatus::ACTIVE;
	state->routes[route.root_tag] = route;
	auto registry = std::make_shared<RNNativeModuleRegistry>(state);
	registry->begin_generation(generation);
	RNEventSchema changed;
	changed.name = "changed";
	changed.subscription_name = "onChanged";
	changed.payload = RNValueSchema::value(RNValueType::STRING);
	changed.requires_session = true;
	RNModuleDefinition definition;
	definition.name = "EventFixture";
	definition.events.push_back(changed);
	definition.factory = []() { return std::make_unique<CompletedAsyncModule>(); };
	RNError error;
	REQUIRE(registry->register_module(definition, error));
	runtime->install_host_object("__testNativeModules", registry);
	runtime->evaluate(
			"globalThis.eventModule = __testNativeModules.get('EventFixture');"
			"globalThis.eventSession = __testNativeModules.openSession(11);"
			"globalThis.eventCalls = [];"
			"globalThis.eventSubscriptions = [];"
			"undefined;");
	REQUIRE(runtime->get_last_error().is_empty());
	const String session = runtime->get_global("eventSession");
	REQUIRE_FALSE(session.is_empty());

	SUBCASE("a callback removes itself before another queued event") {
		runtime->evaluate(
				"globalThis.selfSubscription = eventModule.onChanged(eventSession, value => {"
				"  eventCalls.push(value);"
				"  selfSubscription.remove();"
				"});"
				"undefined;");
		REQUIRE(runtime->get_last_error().is_empty());
		registry->queue_event("EventFixture", "changed", session, generation, String("first"));
		registry->queue_event("EventFixture", "changed", session, generation, String("second"));
		runtime->dispatch_native_module_deliveries(registry);
		const Array calls = runtime->get_global("eventCalls");
		REQUIRE(calls.size() == 1);
		CHECK(String(calls[0]) == "first");
	}

	SUBCASE("removed listeners do not receive the current event") {
		runtime->evaluate(
				"for (let i = 0; i < 8; ++i) {"
				"  eventSubscriptions.push(eventModule.onChanged(eventSession, value => {"
				"    eventCalls.push(value);"
				"    eventSubscriptions.forEach(subscription => subscription.remove());"
				"  }));"
				"}"
				"undefined;");
		REQUIRE(runtime->get_last_error().is_empty());
		registry->queue_event("EventFixture", "changed", session, generation, String("first"));
		runtime->dispatch_native_module_deliveries(registry);
		CHECK(Array(runtime->get_global("eventCalls")).size() == 1);
	}

	SUBCASE("new listeners begin with the next event") {
		runtime->evaluate(
				"globalThis.selfSubscription = eventModule.onChanged(eventSession, value => {"
				"  eventCalls.push(value);"
				"  selfSubscription.remove();"
				"  for (let i = 0; i < 128; ++i) {"
				"    eventSubscriptions.push(eventModule.onChanged(eventSession, next => eventCalls.push(next)));"
				"  }"
				"});"
				"undefined;");
		REQUIRE(runtime->get_last_error().is_empty());
		registry->queue_event("EventFixture", "changed", session, generation, String("first"));
		runtime->dispatch_native_module_deliveries(registry);
		CHECK(Array(runtime->get_global("eventCalls")).size() == 1);
		registry->queue_event("EventFixture", "changed", session, generation, String("second"));
		runtime->dispatch_native_module_deliveries(registry);
		const Array calls = runtime->get_global("eventCalls");
		REQUIRE(calls.size() == 129);
		for (int index = 1; index < calls.size(); ++index) {
			CHECK(String(calls[index]) == "second");
		}
		runtime->evaluate("eventSubscriptions.forEach(subscription => subscription.remove()); undefined;");
		REQUIRE(runtime->get_last_error().is_empty());
	}

	SUBCASE("closing a session stops its remaining callbacks and queued events") {
		runtime->evaluate(
				"for (let i = 0; i < 8; ++i) {"
				"  eventModule.onChanged(eventSession, value => {"
				"    eventCalls.push(value);"
				"    __testNativeModules.closeSession(eventSession);"
				"  });"
				"}"
				"undefined;");
		REQUIRE(runtime->get_last_error().is_empty());
		registry->queue_event("EventFixture", "changed", session, generation, String("first"));
		registry->queue_event("EventFixture", "changed", session, generation, String("second"));
		runtime->dispatch_native_module_deliveries(registry);
		CHECK(Array(runtime->get_global("eventCalls")).size() == 1);
	}

	CHECK_FALSE(registry->has_pending_work());
	runtime->uninstall_host_object("__testNativeModules");
}

TEST_CASE("[ReactNativeBindings][NativeModules] bounded listener snapshots checkpoint and retain immutable payloads") {
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	auto state = std::make_shared<RNRuntimeCoordinatorState>();
	state->service_settings.limits["scheduler/max_tasks_per_frame"] = 2;
	auto registry = std::make_shared<RNNativeModuleRegistry>(state);
	const uint64_t generation = runtime->get_runtime_generation();
	registry->begin_generation(generation);
	RNModuleDefinition definition;
	definition.name = "BoundedEvents";
	definition.factory = [] { return std::make_unique<CompletedAsyncModule>(); };
	RNEventSchema event;
	event.name = "changed";
	event.subscription_name = "onChanged";
	event.payload = RNValueSchema::value(RNValueType::DYNAMIC);
	definition.events.push_back(event);
	RNError error;
	REQUIRE(registry->register_module(definition, error));
	runtime->install_host_object("__boundedEvents", registry);
	runtime->evaluate("globalThis.callbackCount=0;globalThis.checkpointCount=0;globalThis.payloads=[];for(let i=0;i<5;i++){__boundedEvents.get('BoundedEvents').onChanged(value=>{payloads.push(value.value);if(checkpointCount!==callbackCount)throw Error('checkpoint missing');callbackCount++;Promise.resolve().then(()=>checkpointCount++);});}undefined;");
	Dictionary payload;
	payload["value"] = 1;
	REQUIRE(registry->queue_event("BoundedEvents", "changed", "", generation, payload));
	payload["value"] = 99;
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(int(runtime->get_global("callbackCount")) == 2);
	CHECK(int(runtime->get_global("checkpointCount")) == 2);
	CHECK(registry->has_pending_work());
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(int(runtime->get_global("callbackCount")) == 4);
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(int(runtime->get_global("callbackCount")) == 5);
	CHECK(runtime->evaluate("payloads.every(value=>value===1)") == Variant(true));
	runtime->uninstall_host_object("__boundedEvents");
}
class OrderedCompletionModule : public CompletedAsyncModule {
public:
	void start_async(const StringName &, const Array &, const RNCallContext &, const RNCompletionToken &p_completion) override {
		p_completion.emit("changed", String("before"));
		p_completion.complete(String("done"));
		p_completion.emit("changed", String("after"));
	}
};
TEST_CASE("[ReactNativeBindings][NativeModules] native event and completion order survives promise checkpoints") {
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	auto registry = std::make_shared<RNNativeModuleRegistry>(std::shared_ptr<RNRuntimeCoordinatorState>());
	registry->begin_generation(runtime->get_runtime_generation());
	RNModuleDefinition definition;
	definition.name = "OrderedCompletion";
	definition.factory = [] { return std::make_unique<OrderedCompletionModule>(); };
	RNMethodSchema method;
	method.name = "work";
	method.mode = RNCallMode::ASYNC;
	method.result = RNValueSchema::value(RNValueType::STRING);
	definition.methods.push_back(method);
	RNEventSchema event;
	event.name = "changed";
	event.subscription_name = "onChanged";
	event.payload = RNValueSchema::value(RNValueType::STRING);
	definition.events.push_back(event);
	RNError error;
	REQUIRE(registry->register_module(definition, error));
	runtime->install_host_object("__orderedCompletion", registry);
	runtime->evaluate("globalThis.deliveryOrder=[];var ordered=__orderedCompletion.get('OrderedCompletion');var subscription=ordered.onChanged(value=>deliveryOrder.push(value));ordered.work().then(value=>{deliveryOrder.push(value);subscription.remove();});undefined;");
	registry->process_jobs();
	runtime->dispatch_native_module_deliveries(registry);
	CHECK(runtime->evaluate("JSON.stringify(deliveryOrder)") == Variant("[\"before\",\"done\"]"));
	runtime->uninstall_host_object("__orderedCompletion");
}
} // namespace TestRNNativeModuleRegistry

void rn_force_link_scheduler_tests();
void rn_force_link_service_tests();

void rn_force_link_native_module_registry_tests() {
	rn_force_link_scheduler_tests();
	rn_force_link_service_tests();
}
