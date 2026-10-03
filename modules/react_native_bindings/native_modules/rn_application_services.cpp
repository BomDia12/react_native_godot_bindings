#include "rn_application_services.h"

#include "../root_view/react_native_root_view.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/os/os.h"
#include "core/string/translation_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"
#include "servers/text/text_server.h"

#include <cmath>

namespace {
class RNApplicationServices : public RNNativeModule {
	std::weak_ptr<RNRuntimeCoordinatorState> state;
	RNCallContext context;
	Dictionary previous_dimensions;
	String previous_scheme;
	String previous_app_state;
	String override_scheme;
	bool previous_focus = true;
	int previous_keyboard_height = 0;

	Dictionary metrics(const Size2 &p_size, double p_scale, double p_font_scale) const {
		Dictionary value;
		value["width"] = p_size.x;
		value["height"] = p_size.y;
		value["scale"] = p_scale;
		value["fontScale"] = p_font_scale;
		return value;
	}
	Dictionary dimensions() const {
		auto shared = state.lock();
		SceneTree *tree = SceneTree::get_singleton();
		Window *window = tree ? tree->get_root() : nullptr;
		Size2 size = window ? window->get_visible_rect().size : Size2(ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_width", 1152), ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_height", 648));
		double scale = window ? window->get_final_transform().get_scale().x : 1;
		if (!std::isfinite(scale) || scale <= 0) {
			scale = 1;
		}
		DisplayServer *display = DisplayServer::get_singleton();
		Size2 screen = size;
		double screen_scale = scale;
		if (display && display->get_name() != "headless") {
			screen = display->screen_get_size();
			screen_scale = display->screen_get_scale();
			if (screen_scale <= 0 || !std::isfinite(screen_scale)) {
				screen_scale = 1;
			}
			screen /= screen_scale;
		}
		Dictionary result;
		result["window"] = metrics(size, scale, shared ? shared->font_scale : 1);
		result["screen"] = metrics(screen, screen_scale, shared ? shared->font_scale : 1);
		return result;
	}
	String scheme() const {
		if (!override_scheme.is_empty()) {
			return override_scheme;
		}
		auto shared = state.lock();
		DisplayServer *display = DisplayServer::get_singleton();
		if (shared && shared->service_settings.follow_system && display && display->is_dark_mode_supported()) {
			return display->is_dark_mode() ? "dark" : "light";
		}
		return shared ? shared->service_settings.color_scheme : String("light");
	}
	void emit(const String &p_name, const Variant &p_payload) {
		if (auto registry = context.registry.lock()) {
			Dictionary event;
			event["name"] = p_name;
			event["payload"] = p_payload;
			registry->queue_event("GodotServices", "event", "", context.generation, event);
		}
	}
	void invalidate() {
		auto shared = state.lock();
		if (!shared) {
			return;
		}
		++shared->metrics_revision;
		for (const auto &entry : shared->routes) {
			auto root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(entry.second.root_view_id));
			if (root) {
				root->_invalidate_host_layout();
			}
		}
	}

public:
	explicit RNApplicationServices(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) : state(p_state) {}
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_args, const RNCallContext &p_context) override {
		context = p_context;
		auto shared = state.lock();
		if (!shared) {
			return RNModuleResult::failure(RNError::make(RNErrorCode::RUNTIME_RESET, "application services are closed", "GodotServices"));
		}
		DisplayServer *display = DisplayServer::get_singleton();
		if (p_method == "clipboardGet" || p_method == "clipboardSet") {
			if (!display || !display->has_feature(DisplayServerEnums::FEATURE_CLIPBOARD)) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "display backend has no clipboard", "Clipboard"));
			}
			if (p_method == "clipboardGet") {
				return RNModuleResult::success(display->clipboard_get());
			}
			display->clipboard_set(p_args[0]);
			return RNModuleResult::success();
		}
		if (p_method == "openURL") {
			const String url = p_args[0];
			if (url.is_empty() || url.find(":") <= 0 || url.contains("\n") || url.contains("\r")) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "URL requires a scheme", "Linking.openURL"));
			}
			if (!display || display->get_name() == "headless" || OS::get_singleton()->shell_open(url) != OK) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "host cannot open this URL", "Linking.openURL"));
			}
			return RNModuleResult::success();
		}
		if (p_method == "vibrate" || p_method == "cancelVibration") {
			if (!OS::get_singleton()->has_feature("mobile")) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "host has no handheld vibration provider", "Vibration"));
			}
			const int duration = p_method == "vibrate" ? int(p_args[0]) : 0;
			if (duration < 0 || duration > 60000) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "vibration duration must be in [0,60000]", "Vibration"));
			}
			OS::get_singleton()->vibrate_handheld(duration);
			return RNModuleResult::success();
		}
		if (p_method == "getState") {
			Dictionary value;
			value["Dimensions"] = dimensions();
			value["colorScheme"] = scheme();
			value["initialAppState"] = shared->application_paused ? "background" : "active";
			Dictionary direction;
			direction["isRTL"] = shared->is_rtl;
			direction["doLeftAndRightSwapInRTL"] = shared->swap_rtl;
			value["direction"] = direction;
			return RNModuleResult::success(value);
		}
		if (p_method == "setColorScheme") {
			const String selected = p_args[0];
			if (selected != "light" && selected != "dark" && selected != "auto" && selected != "unspecified") {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "invalid color scheme", "Appearance.setColorScheme"));
			}
			override_scheme = selected == "auto" || selected == "unspecified" ? String() : selected;
			process_frame(0);
			return RNModuleResult::success();
		}
		if (p_method == "setFontScale") {
			const double scale = p_args[0];
			if (!std::isfinite(scale) || scale <= 0 || scale > 16) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "font scale must be in (0,16]", "GodotServices.setFontScale"));
			}
			if (shared->font_scale != scale) {
				shared->font_scale = scale;
				invalidate();
			}
			process_frame(0);
			return RNModuleResult::success();
		}
		if (p_method == "setDirection") {
			const String option = p_args[0];
			if (option != "allow_rtl" && option != "force_rtl" && option != "swap_rtl") {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "unknown direction option", "I18nManager"));
			}
			Ref<ConfigFile> preferences;
			preferences.instantiate();
			const Error loaded = preferences->load("user://react_native_direction.cfg");
			if (loaded != OK && loaded != ERR_FILE_NOT_FOUND) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::NATIVE, "could not load direction preferences", "I18nManager"));
			}
			preferences->set_value("direction", option, p_args[1]);
			if (preferences->save("user://react_native_direction.cfg") != OK) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::NATIVE, "could not save direction preferences", "I18nManager"));
			}
			return RNModuleResult::success();
		}
		return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "unknown application service method", "GodotServices." + String(p_method)));
	}
	void process_frame(double) override {
		if (context.generation == 0) {
			return;
		}
		auto shared = state.lock();
		if (!shared) {
			return;
		}
		const bool rtl = shared->force_rtl || (shared->allow_rtl && TextServerManager::get_singleton()->get_primary_interface()->is_locale_right_to_left(TranslationServer::get_singleton()->get_locale()));
		if (rtl != shared->is_rtl) {
			shared->is_rtl = rtl;
			invalidate();
		}
		const Dictionary current = dimensions();
		if (current != previous_dimensions) {
			const bool had_metrics = !previous_dimensions.is_empty();
			previous_dimensions = current;
			emit("didUpdateDimensions", current);
			if (had_metrics) {
				invalidate();
			}
		}
		const String current_scheme = scheme();
		if (current_scheme != previous_scheme) {
			previous_scheme = current_scheme;
			Dictionary payload;
			payload["colorScheme"] = current_scheme;
			emit("appearanceChanged", payload);
			invalidate();
		}
		const String app_state = shared->application_paused ? "background" : "active";
		if (app_state != previous_app_state) {
			previous_app_state = app_state;
			Dictionary payload;
			payload["app_state"] = app_state;
			emit("appStateDidChange", payload);
		}
		DisplayServer *display = DisplayServer::get_singleton();
		const int keyboard_height = display && display->has_feature(DisplayServerEnums::FEATURE_VIRTUAL_KEYBOARD) ? display->virtual_keyboard_get_height() : 0;
		if (keyboard_height != previous_keyboard_height) {
			previous_keyboard_height = keyboard_height;
			const Dictionary window = current["window"];
			const double scale = window["scale"];
			Dictionary coordinates;
			coordinates["screenX"] = 0;
			coordinates["screenY"] = std::max(0.0, double(window["height"]) - keyboard_height / scale);
			coordinates["width"] = window["width"];
			coordinates["height"] = keyboard_height / scale;
			Dictionary event;
			event["endCoordinates"] = coordinates;
			event["duration"] = 0;
			event["easing"] = "keyboard";
			emit(keyboard_height > 0 ? "keyboardDidShow" : "keyboardDidHide", event);
		}
		const bool focus = !display || display->get_name() == "headless" || display->window_is_focused();
		if (focus != previous_focus) {
			previous_focus = focus;
			emit("appStateFocusChange", focus);
		}
	}
};
} //namespace
bool rn_register_application_services(RNNativeModuleRegistry &p_registry, const std::shared_ptr<RNRuntimeCoordinatorState> &p_state, RNError &r_error) {
	GLOBAL_DEF("react_native/i18n/allow_rtl", true);
	GLOBAL_DEF("react_native/i18n/force_rtl", false);
	GLOBAL_DEF("react_native/i18n/swap_rtl", true);
	RNModuleDefinition definition;
	definition.name = "GodotServices";
	definition.factory = [p_state] { return std::make_unique<RNApplicationServices>(p_state); };
	for (const char *name : { "getState", "setColorScheme", "setFontScale", "setDirection", "clipboardGet", "clipboardSet", "openURL", "vibrate", "cancelVibration" }) {
		RNMethodSchema method;
		method.name = name;
		method.result = RNValueSchema::value(String(name) == "getState" ? RNValueType::DYNAMIC : String(name) == "clipboardGet" ? RNValueType::STRING
																																: RNValueType::VOID);
		if (String(name) != "getState" && String(name) != "clipboardGet" && String(name) != "cancelVibration") {
			RNArgumentSchema arg;
			arg.name = "value";
			arg.value = RNValueSchema::value(String(name) == "setFontScale" ? RNValueType::FLOAT : String(name) == "vibrate" ? RNValueType::INTEGER
																															 : RNValueType::STRING);
			method.arguments.push_back(arg);
		}
		if (String(name) == "setDirection") {
			RNArgumentSchema flag;
			flag.name = "enabled";
			flag.value = RNValueSchema::value(RNValueType::BOOL);
			method.arguments.push_back(flag);
		}
		definition.methods.push_back(method);
	}
	RNEventSchema event;
	event.name = "event";
	event.subscription_name = "onEvent";
	event.payload = RNValueSchema::value(RNValueType::DYNAMIC);
	definition.events.push_back(event);
	return p_registry.register_module(definition, r_error);
}
