#pragma once

#include "../interop/rn_schema.h"

#include "tests/test_macros.h"

#include <limits>

namespace TestRNSchemas {

TEST_CASE("[ReactNativeBindings][Schema] closed records validate exact native fields") {
	RNRecordFieldSchema value;
	value.name = "value";
	value.value = std::make_shared<RNValueSchema>(RNValueSchema::value(RNValueType::FLOAT));
	RNValueSchema schema = RNValueSchema::record({ value });
	RNError error;
	CHECK(rn_validate_value_schema(schema, error));
	Dictionary valid;
	valid["value"] = 1.5;
	CHECK(rn_validate_native_value(valid, schema, error, "record"));
	Dictionary extra = valid.duplicate();
	extra["unexpected"] = true;
	CHECK_FALSE(rn_validate_native_value(extra, schema, error, "record"));
	CHECK(error.code == RNErrorCode::VALIDATION);
}

TEST_CASE("[ReactNativeBindings][Schema] invalid defaults fail registration validation") {
	RNArgumentSchema argument;
	argument.name = "count";
	argument.value = RNValueSchema::value(RNValueType::INTEGER);
	argument.has_default = true;
	argument.default_value = 1.5;
	RNMethodSchema method;
	method.name = "invalid";
	method.arguments.push_back(argument);
	RNError error;
	CHECK_FALSE(rn_validate_method_schema(method, error));
}

TEST_CASE("[ReactNativeBindings][Schema] integer defaults respect the JavaScript safe-integer range") {
	constexpr int64_t MAX_SAFE_INTEGER = 9007199254740991;
	for (int64_t value : { -MAX_SAFE_INTEGER, MAX_SAFE_INTEGER, -MAX_SAFE_INTEGER - 1, MAX_SAFE_INTEGER + 1, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max() }) {
		const Variant native_value(value);
		CHECK(int64_t(native_value) == value);
		const bool safe = value >= -MAX_SAFE_INTEGER && value <= MAX_SAFE_INTEGER;
		RNArgumentSchema argument;
		argument.name = "count";
		argument.value = RNValueSchema::value(RNValueType::INTEGER);
		argument.has_default = true;
		argument.default_value = native_value;
		RNMethodSchema method;
		method.name = "count";
		method.arguments.push_back(argument);
		RNError error;
		CHECK(rn_validate_method_schema(method, error) == safe);
		if (!safe) {
			CHECK(error.code == RNErrorCode::VALIDATION);
			CHECK(error.path == "method.args.count.default");
		}

		RNRecordFieldSchema field;
		field.name = "count";
		field.value = std::make_shared<RNValueSchema>(argument.value);
		field.has_default = true;
		field.default_value = native_value;
		error = RNError();
		CHECK(rn_validate_value_schema(RNValueSchema::record({ field }), error) == safe);
		CHECK(rn_validate_native_value(native_value, RNValueSchema::value(RNValueType::INT64), error, "integer"));
	}
}

TEST_CASE("[ReactNativeBindings][Schema] null defaults honor argument and record-field nullability") {
	for (bool value_nullable : { false, true }) {
		for (bool member_nullable : { false, true }) {
			RNArgumentSchema argument;
			argument.name = "value";
			argument.value = RNValueSchema::value(RNValueType::INTEGER);
			argument.value.nullable = value_nullable;
			argument.nullable = member_nullable;
			argument.has_default = true;
			RNMethodSchema method;
			method.name = "nullable";
			method.arguments.push_back(argument);
			RNError error;
			CHECK(rn_validate_method_schema(method, error) == (value_nullable || member_nullable));

			RNRecordFieldSchema field;
			field.name = "value";
			field.value = std::make_shared<RNValueSchema>(argument.value);
			field.nullable = member_nullable;
			field.has_default = true;
			error = RNError();
			CHECK(rn_validate_value_schema(RNValueSchema::record({ field }), error) == (value_nullable || member_nullable));
		}
	}
}

} // namespace TestRNSchemas
