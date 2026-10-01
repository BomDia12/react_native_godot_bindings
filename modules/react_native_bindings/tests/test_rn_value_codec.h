#pragma once

#include "../interop/rn_schema.h"

#include "tests/test_macros.h"

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

} // namespace TestRNValueCodec
