#pragma once

#include "../interop/rn_error.h"

#include "core/templates/hash_map.h"

struct RNServiceSettings {
	HashMap<String, int64_t> limits;
	double font_scale = 1;
	String color_scheme = "light";
	bool follow_system = false;
	int64_t limit(const String &p_name) const {
		const int64_t *value = limits.getptr(p_name);
		return value ? *value : 0;
	}
	static bool snapshot(RNServiceSettings &r_settings, RNError &r_error);
};
void rn_register_service_settings();
