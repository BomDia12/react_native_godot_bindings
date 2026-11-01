#pragma once

#include "rn_host_module_context.h"

#include "core/string/ustring.h"

#include <ReactNativeGodotSpec/ReactNativeGodotSpecJSI.h>

#include <memory>

class RNGameServicesHandle;

namespace facebook::react {

class RNGodotInputModule : public NativeGodotInputCxxSpec<RNGodotInputModule>, public std::enable_shared_from_this<RNGodotInputModule> {
	RNHostModuleContext host;
	std::shared_ptr<RNGameServicesHandle> services;

	RNGodotInputModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services);
	void _connect();
	void _emit_joypads();
	void _require_action(jsi::Runtime &rt, const String &p_action);

public:
	static std::shared_ptr<RNGodotInputModule> create(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services);
	void watchAction(jsi::Runtime &rt, jsi::String name);
	void unwatchAction(jsi::Runtime &rt, jsi::String name);
	jsi::Value getActions(jsi::Runtime &rt);
	jsi::Value getActionEvents(jsi::Runtime &rt, jsi::String name);
	void setActionEvents(jsi::Runtime &rt, jsi::String name, jsi::Value events);
	void resetAction(jsi::Runtime &rt, jsi::String name);
	jsi::Value captureNextEvent(jsi::Runtime &rt, jsi::Value devices);
	void cancelCapture(jsi::Runtime &rt);
	jsi::String getLastDevice(jsi::Runtime &rt);
	jsi::Value getJoypads(jsi::Runtime &rt);
	void vibrateJoypad(jsi::Runtime &rt, double device, double weak, double strong, double durationMs);
	void stopJoypadVibration(jsi::Runtime &rt, double device);
};

class RNGodotAudioModule : public NativeGodotAudioCxxSpec<RNGodotAudioModule>, public std::enable_shared_from_this<RNGodotAudioModule> {
	RNHostModuleContext host;
	std::shared_ptr<RNGameServicesHandle> services;

	RNGodotAudioModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services);
	void _connect();
	void _emit_buses();

public:
	static std::shared_ptr<RNGodotAudioModule> create(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services);
	void play(jsi::Runtime &rt, jsi::String source, jsi::String bus, double volumeDb, double pitchScale);
	jsi::Value preload(jsi::Runtime &rt, jsi::Value sources);
	void stopAll(jsi::Runtime &rt, std::optional<jsi::String> bus);
	jsi::Value getBuses(jsi::Runtime &rt);
	void setBusVolume(jsi::Runtime &rt, jsi::String name, double linear);
	void setBusMute(jsi::Runtime &rt, jsi::String name, bool muted);
};

class RNGodotTranslationModule : public NativeGodotTranslationCxxSpec<RNGodotTranslationModule>, public std::enable_shared_from_this<RNGodotTranslationModule> {
	RNHostModuleContext host;
	std::shared_ptr<RNGameServicesHandle> services;

	RNGodotTranslationModule(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services);
	void _connect();

public:
	static std::shared_ptr<RNGodotTranslationModule> create(std::shared_ptr<CallInvoker> jsInvoker, RNHostModuleContext p_host, std::shared_ptr<RNGameServicesHandle> p_services);
	jsi::String translate(jsi::Runtime &rt, jsi::String message, jsi::String context);
	jsi::String translatePlural(jsi::Runtime &rt, jsi::String message, jsi::String plural, double n, jsi::String context);
	jsi::String getLocale(jsi::Runtime &rt);
	void setLocale(jsi::Runtime &rt, jsi::String locale);
	jsi::Array getLoadedLocales(jsi::Runtime &rt);
};

} // namespace facebook::react
