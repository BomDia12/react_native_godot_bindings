#pragma once

#include "../root_view/react_native_root_view.h"
#include "rn_host_module_context.h"

#include <ReactNativeGodotSpec/ReactNativeGodotSpecJSI.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

// What the GodotHost module shares with Godot: root state changes, which
// reach every module instance, and whether JavaScript handles back presses.
class RNGodotHostEvents {
	std::mutex mutex;
	std::vector<std::function<bool(int, const RNRootState &)>> listeners;
	std::atomic<bool> back_handler_enabled = false;

public:
	// Each module instance listens until its listener reports that it is gone.
	void add_listener(std::function<bool(int p_root_tag, const RNRootState &p_state)> p_listener);
	void root_state_changed(int p_root_tag, const RNRootState &p_state);
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
	void invokeDefaultBackPressHandler(jsi::Runtime &rt);
	void emit_root_state(int p_root_tag, const RNRootState &p_state);
};

} // namespace facebook::react
