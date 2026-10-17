#include "../interop/rn_value_codec.h"
#include "support/rn_test_runtime.h"
#include "support/rn_upstream_test_macros.h"

#include <cmath>
#include <limits>

namespace TestRNValueCodec {

using namespace facebook;

const RNValueSchema DYNAMIC = RNValueSchema::value(RNValueType::DYNAMIC);

TEST_CASE("[ReactNativeBindings][ValueCodec] typed native values preserve lossless shapes") {
	RNError error;
	CHECK(rn_validate_native_value(int64_t(INT64_MAX), RNValueSchema::value(RNValueType::INT64), error, "integer"));
	CHECK(rn_validate_native_value(PackedByteArray({ 1, 2, 3 }), RNValueSchema::value(RNValueType::BYTES), error, "bytes"));
	CHECK(rn_validate_native_value(::Color(0.1, 0.2, 0.3, 0.4), RNValueSchema::value(RNValueType::COLOR), error, "color"));
	CHECK_FALSE(rn_validate_native_value(::Color(2, 0, 0, 1), RNValueSchema::value(RNValueType::COLOR), error, "color"));
	CHECK_FALSE(rn_validate_native_value(::Color(std::numeric_limits<float>::quiet_NaN(), 0, 0, 1), RNValueSchema::value(RNValueType::COLOR), error, "color"));
	CHECK_FALSE(rn_validate_native_value(Vector2(std::numeric_limits<float>::infinity(), 0), RNValueSchema::value(RNValueType::VECTOR2), error, "vector"));
	CHECK_FALSE(rn_validate_native_value(Rect2(0, 0, -1, 2), RNValueSchema::value(RNValueType::RECT2), error, "rect"));
}

TEST_CASE("[ReactNativeBindings][ValueCodec] native records reject non-string keys") {
	RNTestRuntime runtime;
	Dictionary record;
	record[int64_t(1)] = "numeric";
	record["1"] = "string";
	jsi::Value result;
	RNError error;
	CHECK_FALSE(RNValueCodec::to_js(runtime, record, DYNAMIC, result, error));
	CHECK(error.message.contains("record keys must be strings"));
}

TEST_CASE("[ReactNativeBindings][ValueCodec] empty object handles reject the entire conversion") {
	RNTestRuntime runtime;
	for (const char *source : { "({$godot: 'Object', handle: ''})", "({valid: 1, target: {$godot: 'Object', handle: ''}})" }) {
		const RNDecodedValue decoded = RNValueCodec::from_js(runtime, runtime.evaluate(source), DYNAMIC, "test");
		CHECK(decoded.error.code == RNErrorCode::VALIDATION);
		CHECK(decoded.error.message.contains("handle"));
	}
	const RNDecodedValue valid = RNValueCodec::from_js(runtime, runtime.evaluate("({$godot: 'Object', handle: 'opaque-token'})"), DYNAMIC, "test");
	REQUIRE(valid.ok());
	CHECK(String(valid.value) == "opaque-token");
}

TEST_CASE("[ReactNativeBindings][ValueCodec] oversized native strings preserve payload-limit errors") {
	RNTestRuntime runtime;
	jsi::Value result;
	RNError error;
	CHECK_FALSE(RNValueCodec::to_js(runtime, String("x").repeat(16 * 1024 * 1024 + 1), DYNAMIC, result, error));
	CHECK(error.code == RNErrorCode::LIMIT);
	CHECK(error.message.contains("maximum aggregate payload"));
	error = RNError();
	CHECK(RNValueCodec::to_js(runtime, String("valid"), DYNAMIC, result, error));
	CHECK(result.asString(runtime).utf8(runtime) == "valid");
}

TEST_CASE("[ReactNativeBindings][ValueCodec] dynamic safe integers retain native integer typing without rounding") {
	RNTestRuntime runtime;
	auto decode = [&runtime](const char *p_source) { return RNValueCodec::from_js(runtime, runtime.evaluate(p_source), DYNAMIC, "test").value; };
	CHECK(decode("7").get_type() == Variant::INT);
	CHECK(decode("7.5").get_type() == Variant::FLOAT);
	const Variant negative_zero = decode("-0");
	CHECK(negative_zero.get_type() == Variant::FLOAT);
	CHECK(std::signbit(double(negative_zero)));
	CHECK(decode("9007199254740992").get_type() == Variant::FLOAT);
}

} // namespace TestRNValueCodec

void rn_force_link_value_codec_tests() {}
