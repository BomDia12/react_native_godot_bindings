#pragma once

#include "../root_view/react_native_root_view.h"
#include "rn_host_module_context.h"

#include <ReactNativeGodotSpec/ReactNativeGodotSpecJSI.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace facebook::react {
class RNGodotHostModule;
}

// What the GodotHost module shares with Godot: root state changes and
// messages, which reach every module instance, and whether JavaScript handles
// back presses.
class RNGodotHostEvents {
	std::mutex mutex;
	std::vector<std::weak_ptr<facebook::react::RNGodotHostModule>> modules;
	std::atomic<bool> back_handler_enabled = false;

	std::vector<std::shared_ptr<facebook::react::RNGodotHostModule>> _live_modules();

public:
	void add_module(std::weak_ptr<facebook::react::RNGodotHostModule> p_module);
	void root_state_changed(int p_root_tag, const RNRootState &p_state);
	// An empty root tag addresses the application instead of one root.
	void send_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload);
	void set_back_handler_enabled(bool p_enabled) { back_handler_enabled = p_enabled; }
	bool is_back_handler_enabled() const { return back_handler_enabled; }
};

namespace facebook::react {

class RNGodotHostModule : public NativeGodotHostCxxSpec<RNGodotHostModule> {
	RNHostModuleContext host;
	std::shared_ptr<RNGodotHostEvents> events;

public:
	RNGodotHostModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGodotHostEvents> p_events);
	jsi::Object getRootState(jsi::Runtime &rt, double rootTag);
	void setBackHandlerEnabled(jsi::Runtime &rt, bool enabled);
	void postMessage(jsi::Runtime &rt, std::optional<double> rootTag, jsi::String name, jsi::Value payload);
	void emit_root_state(int p_root_tag, const RNRootState &p_state);
	void emit_message(std::optional<int> p_root_tag, const String &p_name, const Variant &p_payload);
};

} // namespace facebook::react
