#pragma once

#include "../interop/rn_schema.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "tests/test_macros.h"

#include <cmath>
#include <limits>

namespace TestRNValueCodec {

TEST_CASE("[ReactNativeBindings][ValueCodec] typed native values preserve lossless shapes") {
	RNError error;
	CHECK(rn_validate_native_value(int64_t(INT64_MAX), RNValueSchema::value(RNValueType::INT64), error, "integer"));
	CHECK(rn_validate_native_value(PackedByteArray({ 1, 2, 3 }), RNValueSchema::value(RNValueType::BYTES), error, "bytes"));
	CHECK(rn_validate_native_value(Color(0.1, 0.2, 0.3, 0.4), RNValueSchema::value(RNValueType::COLOR), error, "color"));
	CHECK_FALSE(rn_validate_native_value(Color(2, 0, 0, 1), RNValueSchema::value(RNValueType::COLOR), error, "color"));
	CHECK_FALSE(rn_validate_native_value(Color(std::numeric_limits<float>::quiet_NaN(), 0, 0, 1), RNValueSchema::value(RNValueType::COLOR), error, "color"));
	CHECK_FALSE(rn_validate_native_value(Vector2(std::numeric_limits<float>::infinity(), 0), RNValueSchema::value(RNValueType::VECTOR2), error, "vector"));
	CHECK_FALSE(rn_validate_native_value(Rect2(0, 0, -1, 2), RNValueSchema::value(RNValueType::RECT2), error, "rect"));
}

TEST_CASE("[ReactNativeBindings][ValueCodec] native records reject non-string keys") {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime != nullptr);
	runtime->reset();

	Dictionary record;
	record[int64_t(1)] = "numeric";
	record["1"] = "string";
	ERR_PRINT_OFF;
	runtime->set_global("invalidRecord", record);
	ERR_PRINT_ON;
	CHECK(runtime->get_last_error().contains("record keys must be strings"));
	CHECK(String(runtime->evaluate("typeof invalidRecord")) == "undefined");
}

TEST_CASE("[ReactNativeBindings][ValueCodec] empty object and session handles reject the entire conversion") {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime != nullptr);
	runtime->reset();

	for (const char *wrapper : { "Object", "Session" }) {
		const String empty_handle = vformat("({$godot: '%s', handle: ''})", wrapper);
		for (const String &source : { empty_handle, vformat("({valid: 1, target: %s})", empty_handle) }) {
			ERR_PRINT_OFF;
			CHECK(runtime->evaluate(source).get_type() == Variant::NIL);
			ERR_PRINT_ON;
			CHECK(runtime->get_last_error().contains("E_VALIDATION"));
			CHECK(runtime->get_last_error().contains("handle"));
		}
		CHECK(String(runtime->evaluate(vformat("({$godot: '%s', handle: 'opaque-token'})", wrapper))) == "opaque-token");
		CHECK(runtime->get_last_error().is_empty());
	}
}

TEST_CASE("[ReactNativeBindings][ValueCodec] oversized native strings preserve payload-limit errors") {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	REQUIRE(runtime != nullptr);
	runtime->reset();

	const String oversized = String("x").repeat(16 * 1024 * 1024 + 1);
	ERR_PRINT_OFF;
	runtime->set_global("oversizedString", oversized);
	ERR_PRINT_ON;
	CHECK(runtime->get_last_error().contains("E_LIMIT"));
	CHECK(runtime->get_last_error().contains("maximum aggregate payload"));
	CHECK_FALSE(runtime->get_last_error().contains("NUL"));
	CHECK(String(runtime->evaluate("typeof oversizedString")) == "undefined");

	runtime->set_global("validString", String("valid"));
	CHECK(runtime->get_last_error().is_empty());
	CHECK(String(runtime->get_global("validString")) == "valid");
}

TEST_CASE("[ReactNativeBindings][ValueCodec] dynamic safe integers retain native integer typing without rounding") {
	auto runtime = HermesRuntimeSingleton::get_singleton();
	runtime->reset();
	CHECK(runtime->evaluate("7").get_type() == Variant::INT);
	CHECK(runtime->evaluate("7.5").get_type() == Variant::FLOAT);
	const Variant negative_zero = runtime->evaluate("-0");
	CHECK(negative_zero.get_type() == Variant::FLOAT);
	CHECK(std::signbit(double(negative_zero)));
	CHECK(runtime->evaluate("9007199254740992").get_type() == Variant::FLOAT);
}
} // namespace TestRNValueCodec
