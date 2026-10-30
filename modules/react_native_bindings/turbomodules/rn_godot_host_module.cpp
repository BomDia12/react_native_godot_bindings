#include "rn_godot_host_module.h"

#include "../host/react_native_host.h"
#include "../host/rn_main_thread_queue.h"
#include "../interop/rn_value_codec.h"
#include "rn_js_promise.h"

std::vector<std::shared_ptr<facebook::react::RNGodotHostModule>> RNGodotHostEvents::_live_modules() {
	std::lock_guard lock(mutex);
	std::vector<std::shared_ptr<facebook::react::RNGodotHostModule>> live;
	std::erase_if(modules, [&live](const std::weak_ptr<facebook::react::RNGodotHostModule> &p_module) {
		auto module = p_module.lock();
		if (!module) {
			return true;
		}
		live.push_back(std::move(module));
		return false;
	});
	return live;
}

void RNGodotHostEvents::add_module(std::weak_ptr<facebook::react::RNGodotHostModule> p_module) {
	std::lock_guard lock(mutex);
	modules.push_back(std::move(p_module));
}

void RNGodotHostEvents::root_state_changed(int p_root_tag, const RNRootState &p_state) {
	for (const auto &module : _live_modules()) {
		module->emit_root_state(p_root_tag, p_state);
	}
}

void RNGodotHostEvents::send_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload) {
	for (const auto &module : _live_modules()) {
		module->emit_message(p_root_tag, p_name, p_payload);
	}
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

jsi::Value root_tag_to_js(std::optional<int> p_root_tag) {
	return p_root_tag ? jsi::Value(*p_root_tag) : jsi::Value::null();
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

// Messages for a root go out through its message_received signal, and
// application messages through ReactNativeHost's.
void RNGodotHostModule::postMessage(jsi::Runtime &rt, std::optional<double> rootTag, jsi::String name, jsi::Value payload) {
	RNDecodedValue decoded = RNValueCodec::from_js(rt, payload, RNValueSchema::value(RNValueType::DYNAMIC), "GodotHost.postMessage", "payload");
	if (!decoded.ok()) {
		rn_throw_js_error(rt, decoded.error.code.utf8().get_data(), decoded.error.message.utf8().get_data());
	}
	const String message = String::utf8(name.utf8(rt).c_str());
	const std::optional<int> root_tag = rootTag ? std::optional<int>(int(*rootTag)) : std::nullopt;
	host.main_thread->post([find_root = host.find_root, root_tag, message, value = decoded.value] {
		Object *target = root_tag ? find_root(*root_tag) : ReactNativeHost::get_singleton();
		if (!target) {
			WARN_PRINT(root_tag ? vformat("The message '%s' was dropped because root %d is not running.", message, *root_tag) : vformat("The message '%s' was dropped because React Native is shutting down.", message));
			return;
		}
		target->emit_signal("message_received", message, value);
	});
}

void RNGodotHostModule::invokeDefaultBackPressHandler(jsi::Runtime &) {
	host.main_thread->post([] {
		if (ReactNativeHost *host_singleton = ReactNativeHost::get_singleton()) {
			host_singleton->invoke_default_back_press();
		}
	});
}

void RNGodotHostModule::emit_root_state(int p_root_tag, const RNRootState &p_state) {
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onRootStateChanged"]).emit([p_root_tag, p_state](jsi::Runtime &p_runtime) -> jsi::Value {
		jsi::Object change = state_to_js(p_runtime, p_state);
		change.setProperty(p_runtime, "rootTag", p_root_tag);
		return change;
	});
}

void RNGodotHostModule::emit_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload) {
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onMessage"]).emit([p_root_tag, p_name, p_payload](jsi::Runtime &p_runtime) -> jsi::Value {
		jsi::Value payload;
		RNError error;
		if (!RNValueCodec::to_js(p_runtime, p_payload, RNValueSchema::value(RNValueType::DYNAMIC), payload, error, nullptr, "postMessage", "payload")) {
			ERR_PRINT(vformat("The message '%s' was dropped: %s", p_name, error.describe()));
			return jsi::Value::null();
		}
		jsi::Object message(p_runtime);
		message.setProperty(p_runtime, "rootTag", root_tag_to_js(p_root_tag));
		message.setProperty(p_runtime, "name", jsi::String::createFromUtf8(p_runtime, p_name.utf8().get_data()));
		message.setProperty(p_runtime, "payload", payload);
		return message;
	});
}

} // namespace facebook::react
