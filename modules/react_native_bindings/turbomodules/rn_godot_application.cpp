#include "rn_godot_application.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/string/translation_server.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display/display_server.h"
#include "servers/text/text_server.h"

#include <cmath>

namespace {

folly::dynamic metrics(const RNDisplayMetrics &p_metrics) {
	return folly::dynamic::object("width", p_metrics.width)("height", p_metrics.height)("scale", p_metrics.scale)("fontScale", p_metrics.font_scale);
}

double finite_scale(double p_scale) {
	return std::isfinite(p_scale) && p_scale > 0 ? p_scale : 1;
}

} // namespace

// Direction preferences apply from the next start, as React Native documents.
RNGodotApplication::RNGodotApplication() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	Ref<ConfigFile> direction;
	direction.instantiate();
	direction->load(DIRECTION_PREFERENCES);
	force_rtl = direction->get_value("direction", "force_rtl", settings->get_setting("react_native/i18n/force_rtl", false));
	allow_rtl = direction->get_value("direction", "allow_rtl", settings->get_setting("react_native/i18n/allow_rtl", true));
	swap_left_and_right = direction->get_value("direction", "swap_rtl", settings->get_setting("react_native/i18n/swap_rtl", true));
}

folly::dynamic RNGodotApplication::dimensions(const RNApplicationSnapshot &p_snapshot) {
	return folly::dynamic::object("window", metrics(p_snapshot.window))("screen", metrics(p_snapshot.screen));
}

RNApplicationSnapshot RNGodotApplication::_capture() const {
	std::string color_scheme;
	bool active;
	{
		std::lock_guard lock(mutex);
		color_scheme = scheme_override;
		active = !paused;
	}
	RNApplicationSnapshot captured;
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const double font_scale = settings->get_setting("react_native/text/font_scale", 1.0);
	SceneTree *tree = SceneTree::get_singleton();
	Window *window = tree ? tree->get_root() : nullptr;
	const Size2 size = window ? window->get_visible_rect().size : Size2(settings->get_setting("display/window/size/viewport_width", 1152), settings->get_setting("display/window/size/viewport_height", 648));
	const double scale = finite_scale(window ? window->get_final_transform().get_scale().x : 1);
	captured.window = { size.x, size.y, scale, font_scale };
	captured.screen = captured.window;
	DisplayServer *display = DisplayServer::get_singleton();
	const bool headless = !display || display->get_name() == "headless";
	if (!headless) {
		const double screen_scale = finite_scale(display->screen_get_scale());
		const Size2 screen = Size2(display->screen_get_size()) / screen_scale;
		captured.screen = { screen.x, screen.y, screen_scale, font_scale };
	}
	if (!color_scheme.empty()) {
		captured.color_scheme = color_scheme;
	} else if (bool(settings->get_setting("react_native/appearance/follow_system", false)) && display && display->is_dark_mode_supported()) {
		captured.color_scheme = display->is_dark_mode() ? "dark" : "light";
	} else {
		captured.color_scheme = String(settings->get_setting("react_native/appearance/color_scheme", "light")).utf8().get_data();
	}
	captured.active = active;
	captured.focused = headless || display->window_is_focused();
	captured.swap_left_and_right = swap_left_and_right;
	const String locale = TranslationServer::get_singleton()->get_locale();
	captured.locale = locale.utf8().get_data();
	captured.is_rtl = force_rtl || (allow_rtl && TextServerManager::get_singleton()->get_primary_interface()->is_locale_right_to_left(locale));
	captured.screen_reader = display && display->accessibility_screen_reader_active() == 1;
	captured.keyboard_height = display && display->has_feature(DisplayServerEnums::FEATURE_VIRTUAL_KEYBOARD) ? display->virtual_keyboard_get_height() / scale : 0;
	return captured;
}

void RNGodotApplication::_emit(const std::string &p_name, folly::dynamic p_payload) const {
	if (emit_event) {
		emit_event(p_name, std::move(p_payload));
	}
}

RNApplicationSnapshot RNGodotApplication::get_snapshot() const {
	std::lock_guard lock(mutex);
	return snapshot;
}

void RNGodotApplication::poll() {
	const RNApplicationSnapshot current = _capture();
	RNApplicationSnapshot previous;
	bool first;
	{
		std::lock_guard lock(mutex);
		previous = snapshot;
		snapshot = current;
		first = !polled;
		polled = true;
	}
	if (first) {
		return;
	}
	if (current.window != previous.window || current.screen != previous.screen) {
		_emit("didUpdateDimensions", dimensions(current));
	}
	if (current.color_scheme != previous.color_scheme) {
		_emit("appearanceChanged", folly::dynamic::object("colorScheme", current.color_scheme));
	}
	if (current.active != previous.active) {
		_emit("appStateDidChange", folly::dynamic::object("app_state", current.active ? "active" : "background"));
	}
	if (current.focused != previous.focused) {
		_emit("appStateFocusChange", current.focused);
	}
	if (current.screen_reader != previous.screen_reader) {
		_emit("screenReaderChanged", current.screen_reader);
	}
	if (current.keyboard_height != previous.keyboard_height) {
		const folly::dynamic coordinates = folly::dynamic::object("screenX", 0)("screenY", std::max(0.0, current.window.height - current.keyboard_height))("width", current.window.width)("height", current.keyboard_height);
		_emit(current.keyboard_height > 0 ? "keyboardDidShow" : "keyboardDidHide", folly::dynamic::object("endCoordinates", coordinates)("duration", 0)("easing", "keyboard"));
	}
}

void RNGodotApplication::set_paused(bool p_paused) {
	std::lock_guard lock(mutex);
	paused = p_paused;
}

void RNGodotApplication::set_color_scheme(const std::string &p_scheme) {
	std::lock_guard lock(mutex);
	scheme_override = p_scheme == "light" || p_scheme == "dark" ? p_scheme : std::string();
}

bool RNGodotApplication::set_direction_preference(const std::string &p_option, bool p_enabled) {
	Ref<ConfigFile> preferences;
	preferences.instantiate();
	const Error loaded = preferences->load(DIRECTION_PREFERENCES);
	if (loaded != OK && loaded != ERR_FILE_NOT_FOUND) {
		return false;
	}
	preferences->set_value("direction", String::utf8(p_option.c_str()), p_enabled);
	return preferences->save(DIRECTION_PREFERENCES) == OK;
}
