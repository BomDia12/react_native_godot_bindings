#include "rn_service_settings.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"

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
	GLOBAL_DEF("react_native/network/tls/trusted_certificates", "");
	settings->set_custom_property_info(PropertyInfo(Variant::STRING, "react_native/network/tls/trusted_certificates", PROPERTY_HINT_FILE, "*.crt,*.pem"));
	GLOBAL_DEF("react_native/dev_server/enabled", false);
	GLOBAL_DEF("react_native/dev_server/host", "localhost");
	GLOBAL_DEF("react_native/dev_server/port", 8081);
	settings->set_custom_property_info(PropertyInfo(Variant::INT, "react_native/dev_server/port", PROPERTY_HINT_RANGE, "1,65535,1"));
	GLOBAL_DEF("react_native/dev_server/entry", "index");
	GLOBAL_DEF("react_native/dev_server/fast_refresh", true);
	GLOBAL_DEF("react_native/bundle/segments", PackedStringArray());
	GLOBAL_DEF(PropertyInfo(Variant::STRING, "react_native/project/root", PROPERTY_HINT_DIR), "res://react_native/");
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
	snapshot.trusted_certificates = settings->get_setting("react_native/network/tls/trusted_certificates", "");
	const Variant dev_enabled = settings->get_setting("react_native/dev_server/enabled", false);
	const Variant dev_host = settings->get_setting("react_native/dev_server/host", "localhost");
	const Variant dev_port = settings->get_setting("react_native/dev_server/port", 8081);
	const Variant dev_entry = settings->get_setting("react_native/dev_server/entry", "index");
	if (dev_enabled.get_type() != Variant::BOOL || dev_host.get_type() != Variant::STRING || String(dev_host).is_empty() || dev_port.get_type() != Variant::INT || int64_t(dev_port) < 1 || int64_t(dev_port) > 65535 || dev_entry.get_type() != Variant::STRING || String(dev_entry).is_empty()) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "invalid developer server configuration", "settings.snapshot", "react_native/dev_server");
		return false;
	}
	// Exported games never reach a developer machine's Metro server, and
	// `run-godot --no-packager` runs the bundle file it has just built.
	const bool bundle_file_requested = OS::get_singleton()->get_cmdline_args().find("--react-native-no-dev-server") != nullptr;
	snapshot.dev_server.enabled = bool(dev_enabled) && !OS::get_singleton()->has_feature("template") && !bundle_file_requested;
	snapshot.dev_server.host = dev_host;
	snapshot.dev_server.port = dev_port;
	snapshot.dev_server.entry = dev_entry;
	snapshot.dev_server.fast_refresh = settings->get_setting("react_native/dev_server/fast_refresh", true);
	snapshot.segments = settings->get_setting("react_native/bundle/segments", PackedStringArray());
	if (snapshot.limit("network/http/max_idle_requests") > snapshot.limit("network/http/max_active_requests")) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "HTTP idle limit exceeds active limit", "settings.snapshot");
		return false;
	}
	if (snapshot.limit("network/http/max_body_bytes") > snapshot.limit("network/http/max_buffered_bytes")) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "HTTP body limit exceeds aggregate buffer limit", "settings.snapshot", "react_native/network/http/max_body_bytes");
		return false;
	}
	r_settings = snapshot;
	return true;
}

bool rn_is_tooling_run() {
	for (const String &argument : OS::get_singleton()->get_cmdline_args()) {
		if (argument == "--react-native-generate-contracts" || argument.begins_with("--react-native-enable-plugins=") || argument.begins_with("--react-native-disable-plugins=")) {
			return true;
		}
	}
	return false;
}

PackedStringArray rn_tooling_values(const String &p_flag) {
	const String prefix = "--" + p_flag + "=";
	for (const String &argument : OS::get_singleton()->get_cmdline_args()) {
		if (argument.begins_with(prefix)) {
			return argument.substr(prefix.length()).split(",", false);
		}
	}
	return PackedStringArray();
}
