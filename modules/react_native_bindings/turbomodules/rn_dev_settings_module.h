#pragma once

#include "rn_host_module_context.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>

namespace facebook::react {

// React Native's DevSettings. A reload replaces the runtime through
// ReactNativeHost.reload() and openDebugger opens DevTools for it; Godot has
// no developer menu, and the other settings do nothing, as upstream's do.
class RNDevSettingsModule : public NativeDevSettingsCxxSpec<RNDevSettingsModule> {
	RNHostModuleContext context;

public:
	RNDevSettingsModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext context);
	void reload(jsi::Runtime &rt);
	void reloadWithReason(jsi::Runtime &rt, const std::string &reason);
	void onFastRefresh(jsi::Runtime &rt) {}
	void setHotLoadingEnabled(jsi::Runtime &rt, bool isHotLoadingEnabled) {}
	void setProfilingEnabled(jsi::Runtime &rt, bool isProfilingEnabled) {}
	void toggleElementInspector(jsi::Runtime &rt) {}
	void addMenuItem(jsi::Runtime &rt, const std::string &title) {}
	void openDebugger(jsi::Runtime &rt);
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
	void setIsShakeToShowDevMenuEnabled(jsi::Runtime &rt, bool enabled) {}
};

} // namespace facebook::react
