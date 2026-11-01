#pragma once

#include "../interop/rn_error.h"

#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

struct RNDevServerSettings {
	bool enabled = false;
	String host = "localhost";
	int64_t port = 8081;
	String entry = "index";
	bool fast_refresh = true;
};

struct RNServiceSettings {
	HashMap<String, int64_t> limits;
	double font_scale = 1;
	String color_scheme = "light";
	bool follow_system = false;
	String trusted_certificates;
	RNDevServerSettings dev_server;
	// Entries of the segments every runtime evaluates after its bundle, in order.
	PackedStringArray segments;
	int64_t limit(const String &p_name) const {
		const int64_t *value = limits.getptr(p_name);
		return value ? *value : 0;
	}
	static bool snapshot(RNServiceSettings &r_settings, RNError &r_error);
};
void rn_register_service_settings();

// An editor started by the CLI to generate contracts or enable plugins, which
// runs no React Native.
bool rn_is_tooling_run();
// The comma-separated values of a `--<flag>=a,b` command-line argument.
PackedStringArray rn_tooling_values(const String &p_flag);
