#pragma once

#include "../interop/rn_schema.h"

#include "tests/test_macros.h"

namespace TestRNSceneSchema {
Dictionary type(const String &p_type) {
	Dictionary schema;
	schema["type"] = p_type;
	return schema;
}
TEST_CASE("[ReactNativeBindings][SceneSchema] unrelated nested dictionaries compile closed schemas and copied defaults") {
	Dictionary reading = type("number");
	reading["default"] = 12.5;
	Dictionary fields;
	fields["reading"] = reading;
	fields["units"] = type("string");
	Dictionary record = type("record");
	record["fields"] = fields;
	RNValueSchema compiled;
	RNError error;
	REQUIRE(rn_parse_value_schema(record, compiled, error));
	Dictionary value;
	value["units"] = "C";
	CHECK(rn_validate_native_value(value, compiled, error, "sensor"));
	value["extra"] = true;
	CHECK_FALSE(rn_validate_native_value(value, compiled, error, "sensor"));
	fields["units"] = type("boolean");
	CHECK(compiled.fields[1].value->type == RNValueType::STRING);
	CHECK(double(compiled.fields[0].default_value) == 12.5);
}
TEST_CASE("[ReactNativeBindings][SceneSchema] unknown misplaced invalid default cyclic and depth bounds fail before attachment") {
	for (const String &name : { String("unknown"), String("array"), String("record") }) {
		RNValueSchema compiled;
		RNError error;
		CHECK_FALSE(rn_parse_value_schema(type(name), compiled, error));
	}
	Dictionary misplaced = type("number");
	misplaced["element"] = type("number");
	RNValueSchema compiled;
	RNError error;
	CHECK_FALSE(rn_parse_value_schema(misplaced, compiled, error));
	Dictionary cyclic = type("array");
	cyclic["element"] = cyclic;
	CHECK_FALSE(rn_parse_value_schema(cyclic, compiled, error));
	cyclic.erase("element");
	Dictionary nested = type("string");
	for (int i = 0; i < 33; ++i) {
		Dictionary array = type("array");
		array["element"] = nested;
		nested = array;
	}
	CHECK_FALSE(rn_parse_value_schema(nested, compiled, error));
	Dictionary invalid = type("integer");
	invalid["default"] = "wrong";
	Dictionary fields;
	fields["value"] = invalid;
	Dictionary record = type("record");
	record["fields"] = fields;
	CHECK_FALSE(rn_parse_value_schema(record, compiled, error));
}
} //namespace TestRNSceneSchema
