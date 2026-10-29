#pragma once

#include "rn_godot_application.h"

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>
#include <ReactNativeGodotSpec/ReactNativeGodotSpecJSI.h>

#include <memory>

class RNMainThreadQueue;

namespace facebook::react {

struct RNApplicationModuleContext {
	std::shared_ptr<RNGodotApplication> application;
	std::shared_ptr<RNMainThreadQueue> main_thread;
};

class RNPlatformConstantsModule : public NativeGodotPlatformConstantsCxxSpec<RNPlatformConstantsModule> {
	bool fast_refresh;

public:
	RNPlatformConstantsModule(std::shared_ptr<CallInvoker> jsInvoker, bool p_fast_refresh) :
			NativeGodotPlatformConstantsCxxSpec(std::move(jsInvoker)), fast_refresh(p_fast_refresh) {}
	jsi::Object getConstants(jsi::Runtime &rt);
};

class RNDeviceInfoModule : public NativeDeviceInfoCxxSpec<RNDeviceInfoModule> {
	RNApplicationModuleContext context;

public:
	RNDeviceInfoModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
};

class RNAppStateModule : public NativeAppStateCxxSpec<RNAppStateModule> {
	RNApplicationModuleContext context;

public:
	RNAppStateModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
	void getCurrentAppState(jsi::Runtime &rt, jsi::Function success, jsi::Function error);
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
};

class RNAppearanceModule : public NativeAppearanceCxxSpec<RNAppearanceModule> {
	RNApplicationModuleContext context;

public:
	RNAppearanceModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	std::string getColorScheme(jsi::Runtime &rt);
	void setColorScheme(jsi::Runtime &rt, const std::string &colorScheme);
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
};

class RNI18nManagerModule : public NativeI18nManagerCxxSpec<RNI18nManagerModule> {
	RNApplicationModuleContext context;
	void _set(const char *p_option, bool p_enabled);

public:
	RNI18nManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
	void allowRTL(jsi::Runtime &rt, bool allowRTL) { _set("allow_rtl", allowRTL); }
	void forceRTL(jsi::Runtime &rt, bool forceRTL) { _set("force_rtl", forceRTL); }
	void swapLeftAndRightInRTL(jsi::Runtime &rt, bool flipStyles) { _set("swap_rtl", flipStyles); }
};

class RNKeyboardObserverModule : public NativeKeyboardObserverCxxSpec<RNKeyboardObserverModule> {
public:
	explicit RNKeyboardObserverModule(std::shared_ptr<CallInvoker> jsInvoker) :
			NativeKeyboardObserverCxxSpec(std::move(jsInvoker)) {}
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
};

class RNClipboardModule : public NativeClipboardCxxSpec<RNClipboardModule> {
	RNApplicationModuleContext context;

public:
	RNClipboardModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
	jsi::Value getString(jsi::Runtime &rt);
	void setString(jsi::Runtime &rt, const std::string &content);
};

class RNLinkingManagerModule : public NativeLinkingManagerCxxSpec<RNLinkingManagerModule> {
	RNApplicationModuleContext context;

public:
	RNLinkingManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	jsi::Value getInitialURL(jsi::Runtime &rt);
	jsi::Value canOpenURL(jsi::Runtime &rt, const std::string &url);
	jsi::Value openURL(jsi::Runtime &rt, const std::string &url);
	jsi::Value openSettings(jsi::Runtime &rt);
	void addListener(jsi::Runtime &rt, const std::string &eventName) {}
	void removeListeners(jsi::Runtime &rt, double count) {}
};

class RNVibrationModule : public NativeVibrationCxxSpec<RNVibrationModule> {
	RNApplicationModuleContext context;

public:
	RNVibrationModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext context);
	jsi::Object getConstants(jsi::Runtime &rt);
	void vibrate(jsi::Runtime &rt, double pattern);
	void vibrateByPattern(jsi::Runtime &rt, jsi::Array pattern, double repeat);
	void cancel(jsi::Runtime &rt);
};

} // namespace facebook::react
