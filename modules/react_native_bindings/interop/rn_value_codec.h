#pragma once

#include "rn_schema.h"

#include "core/templates/vector.h"
#include "core/variant/variant.h"

#include <jsi/jsi.h>

#include <vector>

struct RNConversionLimits {
	uint32_t max_depth = 32;
	uint32_t max_container_entries = 4096;
	uint32_t max_visited_values = 65536;
	uint64_t max_payload_bytes = 16 * 1024 * 1024;
};

struct RNConversionStats {
	uint64_t visited_values = 0;
	uint64_t allocated_containers = 0;
	uint64_t copied_binary_bytes = 0;
	uint64_t payload_bytes = 0;
};

struct RNConversionContext {
	String operation;
	String path;
	RNConversionLimits limits;
	RNConversionStats stats;
	std::vector<facebook::jsi::Object> ancestors;
};

struct RNDecodedValue {
	Variant value;
	RNError error;
	RNConversionStats stats;

	bool ok() const { return !error.is_set(); }
};

class RNValueCodec {
public:
	static RNDecodedValue from_js(facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &p_value, const RNValueSchema &p_schema, const String &p_operation, const String &p_path = "value");
	static bool to_js(facebook::jsi::Runtime &p_runtime, const Variant &p_value, const RNValueSchema &p_schema, facebook::jsi::Value &r_value, RNError &r_error, RNConversionStats *r_stats = nullptr, const String &p_operation = String(), const String &p_path = "value");
};
