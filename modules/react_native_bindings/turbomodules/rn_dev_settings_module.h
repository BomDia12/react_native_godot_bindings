#pragma once

#include "rn_host_module_context.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>

namespace facebook::react {

// React Native's DevSettings. A reload replaces the runtime through
// ReactNativeHost.reload(); the developer menu, debugger and hot loading
// settings do nothing without a development server.
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
	void openDebugger(jsi::Runtime &rt) {}
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
	void setIsShakeToShowDevMenuEnabled(jsi::Runtime &rt, bool enabled) {}
};

} // namespace facebook::react
