#include "rn_service_settings.h"

#include "core/config/project_settings.h"

#include <cmath>

namespace {
struct IntegerSetting {
	const char *name;
	int64_t initial;
	int64_t minimum;
	int64_t maximum;
};
const IntegerSetting integers[] = {
	{ "network/http/max_active_requests", 8, 1, 128 },
	{ "network/http/max_idle_requests", 2, 0, 128 },
	{ "network/http/max_queued_requests", 128, 0, 4096 },
	{ "network/http/max_body_bytes", 8 * 1024 * 1024, 1, INT32_MAX },
	{ "network/http/max_buffered_bytes", 32 * 1024 * 1024, 1, INT32_MAX },
	{ "network/http/max_redirects", 8, 0, 64 },
	{ "network/cookies/max_entries", 256, 0, 4096 },
	{ "network/cookies/max_bytes", 256 * 1024, 0, 16 * 1024 * 1024 },
	{ "network/websocket/max_message_bytes", 1024 * 1024, 1, 16 * 1024 * 1024 - 4096 },
	{ "network/websocket/max_queued_packets", 256, 1, 4096 },
	{ "network/websocket/max_buffered_bytes", 4 * 1024 * 1024, 1, 64 * 1024 * 1024 },
	{ "network/websocket/packets_per_frame", 64, 1, 4096 },
	{ "network/websocket/close_timeout_ms", 2000, 1, 60000 },
	{ "binary/max_blob_bytes", 32 * 1024 * 1024, 1, INT32_MAX },
	{ "scheduler/max_tasks_per_frame", 256, 1, 4096 },
	{ "scheduler/idle_budget_ms", 2, 0, 50 },
};
} //namespace
void rn_register_service_settings() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	for (const auto &entry : integers) {
		const String key = "react_native/" + String(entry.name);
		GLOBAL_DEF(key, entry.initial);
		settings->set_custom_property_info(PropertyInfo(Variant::INT, key, PROPERTY_HINT_RANGE, vformat("%d,%d,1", entry.minimum, entry.maximum)));
	}
	GLOBAL_DEF("react_native/text/font_scale", 1.0);
	settings->set_custom_property_info(PropertyInfo(Variant::FLOAT, "react_native/text/font_scale", PROPERTY_HINT_RANGE, "0.01,16,0.01"));
	GLOBAL_DEF("react_native/appearance/color_scheme", "light");
	settings->set_custom_property_info(PropertyInfo(Variant::STRING, "react_native/appearance/color_scheme", PROPERTY_HINT_ENUM, "light,dark"));
	GLOBAL_DEF("react_native/appearance/follow_system", false);
}
bool RNServiceSettings::snapshot(RNServiceSettings &r_settings, RNError &r_error) {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	RNServiceSettings snapshot;
	for (const auto &entry : integers) {
		const String key = "react_native/" + String(entry.name);
		const Variant value = settings->get_setting(key, entry.initial);
		if (value.get_type() != Variant::INT || int64_t(value) < entry.minimum || int64_t(value) > entry.maximum) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "setting is outside its integer range", "settings.snapshot", key);
			return false;
		}
		snapshot.limits[entry.name] = value;
	}
	const Variant scale = settings->get_setting("react_native/text/font_scale", 1.0);
	if ((scale.get_type() != Variant::FLOAT && scale.get_type() != Variant::INT) || !std::isfinite(double(scale)) || double(scale) <= 0 || double(scale) > 16) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "font scale must be finite and in (0,16]", "settings.snapshot", "react_native/text/font_scale");
		return false;
	}
	snapshot.font_scale = scale;
	snapshot.color_scheme = settings->get_setting("react_native/appearance/color_scheme", "light");
	const Variant follow = settings->get_setting("react_native/appearance/follow_system", false);
	if ((snapshot.color_scheme != "light" && snapshot.color_scheme != "dark") || follow.get_type() != Variant::BOOL) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "invalid appearance configuration", "settings.snapshot");
		return false;
	}
	snapshot.follow_system = follow;
	if (snapshot.limit("network/http/max_idle_requests") > snapshot.limit("network/http/max_active_requests")) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "HTTP idle limit exceeds active limit", "settings.snapshot");
		return false;
	}
	r_settings = snapshot;
	return true;
}
