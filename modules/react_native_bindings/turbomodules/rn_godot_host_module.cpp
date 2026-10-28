#include "rn_godot_host_module.h"

#include "../host/rn_main_thread_queue.h"
#include "rn_js_promise.h"

void RNGodotHostEvents::add_listener(std::function<bool(int, const RNRootState &)> p_listener) {
	std::lock_guard lock(mutex);
	listeners.push_back(std::move(p_listener));
}

void RNGodotHostEvents::root_state_changed(int p_root_tag, const RNRootState &p_state) {
	std::lock_guard lock(mutex);
	std::erase_if(listeners, [&](const std::function<bool(int, const RNRootState &)> &p_listener) { return !p_listener(p_root_tag, p_state); });
}

namespace facebook::react {

namespace {

jsi::Object state_to_js(jsi::Runtime &p_runtime, const RNRootState &p_state) {
	jsi::Object state(p_runtime);
	state.setProperty(p_runtime, "visible", p_state.visible);
	state.setProperty(p_runtime, "processing", p_state.processing);
	state.setProperty(p_runtime, "focused", p_state.focused);
	return state;
}

} // namespace

RNGodotHostModule::RNGodotHostModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGodotHostEvents> p_events) :
		NativeGodotHostCxxSpec(std::move(jsInvoker)), host(std::move(p_host)), events(std::move(p_events)) {
}

jsi::Object RNGodotHostModule::getRootState(jsi::Runtime &rt, double rootTag) {
	RNRootState state;
	bool found = false;
	if (!host.main_thread->run_sync([&] {
			if (ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(host.find_root(int(rootTag)))) {
				state = root->get_state();
				found = true;
			}
		})) {
		rn_throw_js_error(rt, RNErrorCode::CANCELLED, "React Native is shutting down");
	}
	if (!found) {
		rn_throw_js_error(rt, RNErrorCode::STALE_HANDLE, "root is not running");
	}
	return state_to_js(rt, state);
}

void RNGodotHostModule::setBackHandlerEnabled(jsi::Runtime &, bool enabled) {
	events->set_back_handler_enabled(enabled);
}

void RNGodotHostModule::emit_root_state(int p_root_tag, const RNRootState &p_state) {
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onRootStateChanged"]).emit([p_root_tag, p_state](jsi::Runtime &p_runtime) -> jsi::Value {
		jsi::Object change = state_to_js(p_runtime, p_state);
		change.setProperty(p_runtime, "rootTag", p_root_tag);
		return change;
	});
}

} // namespace facebook::react
