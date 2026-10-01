#pragma once

#include "../interop/rn_schema.h"

#include "tests/test_macros.h"

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

} // namespace TestRNSchemas
