#include "rn_application_modules.h"

#include "../host/rn_main_thread_queue.h"
#include "rn_js_promise.h"

#include "core/config/engine.h"
#include "core/os/os.h"
#include "servers/display/display_server.h"

#include <jsi/JSIDynamic.h>

namespace facebook::react {

namespace {

bool display_has(DisplayServerEnums::Feature p_feature) {
	DisplayServer *display = DisplayServer::get_singleton();
	return display && display->has_feature(p_feature);
}

bool display_is_headless() {
	DisplayServer *display = DisplayServer::get_singleton();
	return !display || display->get_name() == "headless";
}

} // namespace

jsi::Object RNPlatformConstantsModule::getConstants(jsi::Runtime &rt) {
	const Dictionary version = Engine::get_singleton()->get_version_info();
	const std::string godot_version = vformat("%d.%d.%d", version.get("major", 0), version.get("minor", 0), version.get("patch", 0)).utf8().get_data();
	double asset_scale = 1;
	if (!display_is_headless()) {
		const double reported = DisplayServer::get_singleton()->screen_get_scale();
		asset_scale = reported > 0 && std::isfinite(reported) ? reported : 1;
	}
#ifdef DEBUG_ENABLED
	constexpr bool testing = true;
#else
	constexpr bool testing = false;
#endif
	const folly::dynamic constants = folly::dynamic::object("isTesting", testing)("reactNativeVersion", folly::dynamic::object("major", 0)("minor", 87)("patch", 1)("prerelease", nullptr))("Version", godot_version)("godotVersion", godot_version)("hostOS", OS::get_singleton()->get_name().utf8().get_data())("assetScale", asset_scale);
	return jsi::valueFromDynamic(rt, constants).asObject(rt);
}

RNDeviceInfoModule::RNDeviceInfoModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeDeviceInfoCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNDeviceInfoModule::getConstants(jsi::Runtime &rt) {
	return jsi::valueFromDynamic(rt, folly::dynamic::object("Dimensions", RNGodotApplication::dimensions(context.application->get_snapshot()))).asObject(rt);
}

RNAppStateModule::RNAppStateModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeAppStateCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNAppStateModule::getConstants(jsi::Runtime &rt) {
	const bool active = context.application->get_snapshot().active;
	return jsi::valueFromDynamic(rt, folly::dynamic::object("initialAppState", active ? "active" : "background")).asObject(rt);
}

void RNAppStateModule::getCurrentAppState(jsi::Runtime &rt, jsi::Function success, jsi::Function) {
	const bool active = context.application->get_snapshot().active;
	success.call(rt, jsi::valueFromDynamic(rt, folly::dynamic::object("app_state", active ? "active" : "background")));
}

RNAppearanceModule::RNAppearanceModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeAppearanceCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

std::string RNAppearanceModule::getColorScheme(jsi::Runtime &) {
	return context.application->get_snapshot().color_scheme;
}

void RNAppearanceModule::setColorScheme(jsi::Runtime &rt, const std::string &colorScheme) {
	if (colorScheme != "light" && colorScheme != "dark" && colorScheme != "unspecified" && colorScheme != "auto") {
		rn_throw_js_error(rt, "E_VALIDATION", "Appearance.setColorScheme expects light, dark or unspecified.");
	}
	context.application->set_color_scheme(colorScheme);
	context.main_thread->post([application = context.application]() { application->poll(); });
}

RNI18nManagerModule::RNI18nManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeI18nManagerCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNI18nManagerModule::getConstants(jsi::Runtime &rt) {
	const RNApplicationSnapshot snapshot = context.application->get_snapshot();
	return jsi::valueFromDynamic(rt, folly::dynamic::object("isRTL", snapshot.is_rtl)("doLeftAndRightSwapInRTL", snapshot.swap_left_and_right)("localeIdentifier", snapshot.locale)).asObject(rt);
}

void RNI18nManagerModule::_set(const char *p_option, bool p_enabled) {
	context.main_thread->post([application = context.application, option = std::string(p_option), p_enabled]() {
		if (!application->set_direction_preference(option, p_enabled)) {
			ERR_PRINT(vformat("I18nManager could not save the %s preference.", String::utf8(option.c_str())));
		}
	});
}

RNClipboardModule::RNClipboardModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeClipboardCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNClipboardModule::getConstants(jsi::Runtime &rt) {
	return jsi::Object(rt);
}

jsi::Value RNClipboardModule::getString(jsi::Runtime &rt) {
	return RNJSPromise::create(rt, jsInvoker_, [main_thread = context.main_thread](std::shared_ptr<RNJSPromise> p_promise) {
		main_thread->post([p_promise]() {
			if (!display_has(DisplayServerEnums::FEATURE_CLIPBOARD)) {
				p_promise->reject("E_UNSUPPORTED", "The display backend has no clipboard.");
				return;
			}
			const std::string text = DisplayServer::get_singleton()->clipboard_get().utf8().get_data();
			p_promise->resolve([text](jsi::Runtime &p_runtime) { return jsi::String::createFromUtf8(p_runtime, text); });
		});
	});
}

void RNClipboardModule::setString(jsi::Runtime &rt, const std::string &content) {
	context.main_thread->post([content]() {
		if (display_has(DisplayServerEnums::FEATURE_CLIPBOARD)) {
			DisplayServer::get_singleton()->clipboard_set(String::utf8(content.c_str()));
		}
	});
}

RNLinkingManagerModule::RNLinkingManagerModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeLinkingManagerCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Value RNLinkingManagerModule::getInitialURL(jsi::Runtime &rt) {
	return RNJSPromise::create(rt, jsInvoker_, [](std::shared_ptr<RNJSPromise> p_promise) { p_promise->resolve([](jsi::Runtime &) { return jsi::Value::null(); }); });
}

jsi::Value RNLinkingManagerModule::canOpenURL(jsi::Runtime &rt, const std::string &) {
	return RNJSPromise::create(rt, jsInvoker_, [](std::shared_ptr<RNJSPromise> p_promise) { p_promise->reject("E_UNSUPPORTED", "Godot has no URL-handler query provider."); });
}

jsi::Value RNLinkingManagerModule::openURL(jsi::Runtime &rt, const std::string &url) {
	return RNJSPromise::create(rt, jsInvoker_, [main_thread = context.main_thread, url](std::shared_ptr<RNJSPromise> p_promise) {
		const String target = String::utf8(url.c_str());
		if (target.is_empty() || target.find(":") <= 0 || target.contains("\n") || target.contains("\r")) {
			p_promise->reject("E_VALIDATION", "Linking.openURL requires a URL with a scheme.");
			return;
		}
		main_thread->post([p_promise, target]() {
			if (display_is_headless() || OS::get_singleton()->shell_open(target) != OK) {
				p_promise->reject("E_UNSUPPORTED", "The host cannot open this URL.");
				return;
			}
			p_promise->resolve([](jsi::Runtime &) { return jsi::Value(true); });
		});
	});
}

jsi::Value RNLinkingManagerModule::openSettings(jsi::Runtime &rt) {
	return RNJSPromise::create(rt, jsInvoker_, [](std::shared_ptr<RNJSPromise> p_promise) { p_promise->reject("E_UNSUPPORTED", "Godot has no application-settings provider."); });
}

RNVibrationModule::RNVibrationModule(std::shared_ptr<CallInvoker> jsInvoker, RNApplicationModuleContext p_context) :
		NativeVibrationCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNVibrationModule::getConstants(jsi::Runtime &rt) {
	return jsi::Object(rt);
}

void RNVibrationModule::vibrate(jsi::Runtime &rt, double pattern) {
	if (!OS::get_singleton()->has_feature("mobile")) {
		rn_throw_js_error(rt, "E_UNSUPPORTED", "The host has no handheld vibration provider.");
	}
	if (!std::isfinite(pattern) || pattern < 0 || pattern > 60000) {
		rn_throw_js_error(rt, "E_VALIDATION", "Vibration duration must be in [0, 60000] milliseconds.");
	}
	context.main_thread->post([duration = int(pattern)]() { OS::get_singleton()->vibrate_handheld(duration); });
}

void RNVibrationModule::vibrateByPattern(jsi::Runtime &rt, jsi::Array pattern, double) {
	if (pattern.size(rt) > 1) {
		vibrate(rt, pattern.getValueAtIndex(rt, 1).asNumber());
	}
}

void RNVibrationModule::cancel(jsi::Runtime &rt) {
	if (OS::get_singleton()->has_feature("mobile")) {
		context.main_thread->post([]() { OS::get_singleton()->vibrate_handheld(0); });
	}
}

} // namespace facebook::react
