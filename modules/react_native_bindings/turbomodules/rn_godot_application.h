#pragma once

#include <folly/dynamic.h>

#include <functional>
#include <mutex>
#include <string>

struct RNDisplayMetrics {
	double width = 0;
	double height = 0;
	double scale = 1;
	double font_scale = 1;

	bool operator==(const RNDisplayMetrics &) const = default;
};

struct RNApplicationSnapshot {
	RNDisplayMetrics window;
	RNDisplayMetrics screen;
	std::string color_scheme = "light";
	bool active = true;
	bool focused = true;
	bool is_rtl = false;
	bool swap_left_and_right = true;
	std::string locale;
	double keyboard_height = 0;
};

// Application-wide state that React Native modules read on the JS thread. The
// main thread refreshes it every frame and reports changes as device events.
class RNGodotApplication {
	mutable std::mutex mutex;
	RNApplicationSnapshot snapshot;
	std::string scheme_override;
	bool paused = false;
	bool force_rtl = false;
	bool allow_rtl = true;
	bool swap_left_and_right = true;
	bool polled = false;
	std::function<void(const std::string &, folly::dynamic)> emit_event;

	RNApplicationSnapshot _capture() const;
	void _emit(const std::string &p_name, folly::dynamic p_payload) const;

public:
	static constexpr const char *DIRECTION_PREFERENCES = "user://react_native_direction.cfg";

	RNGodotApplication();
	void set_event_emitter(std::function<void(const std::string &, folly::dynamic)> p_emit) { emit_event = std::move(p_emit); }
	RNApplicationSnapshot get_snapshot() const;
	void poll();
	void set_paused(bool p_paused);
	void set_color_scheme(const std::string &p_scheme);
	bool set_direction_preference(const std::string &p_option, bool p_enabled);

	static folly::dynamic dimensions(const RNApplicationSnapshot &p_snapshot);
};
