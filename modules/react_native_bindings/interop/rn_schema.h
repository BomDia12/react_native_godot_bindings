#pragma once

#include "rn_error.h"

#include "core/string/string_name.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

#include <functional>
#include <memory>

enum class RNValueType {
	VOID,
	DYNAMIC,
	NULL_VALUE,
	BOOL,
	FLOAT,
	INTEGER,
	STRING,
	ARRAY,
	RECORD,
	BYTES,
	INT64,
	COLOR,
	VECTOR2,
	VECTOR3,
	RECT2,
	TRANSFORM2D,
	OBJECT,
	SESSION,
};

struct RNValueSchema;

struct RNRecordFieldSchema {
	StringName name;
	std::shared_ptr<RNValueSchema> value;
	bool optional = false;
	bool nullable = false;
	bool has_default = false;
	Variant default_value;
};

struct RNValueSchema {
	RNValueType type = RNValueType::DYNAMIC;
	std::shared_ptr<RNValueSchema> element;
	Vector<RNRecordFieldSchema> fields;
	bool closed = true;
	bool nullable = false;
	bool allow_negative_size = false;
	String capability;

	static RNValueSchema value(RNValueType p_type);
	static RNValueSchema array(const RNValueSchema &p_element);
	static RNValueSchema record(const Vector<RNRecordFieldSchema> &p_fields, bool p_closed = true);
	Dictionary to_metadata() const;
};

struct RNArgumentSchema {
	StringName name;
	RNValueSchema value;
	bool optional = false;
	bool nullable = false;
	bool has_default = false;
	Variant default_value;
};

enum class RNCallMode {
	SYNC,
	ASYNC,
};

struct RNMethodSchema {
	StringName name;
	Vector<RNArgumentSchema> arguments;
	RNValueSchema result = RNValueSchema::value(RNValueType::VOID);
	RNCallMode mode = RNCallMode::SYNC;
	bool requires_session = false;
};

struct RNEventSchema {
	StringName name;
	StringName subscription_name;
	RNValueSchema payload;
	bool requires_session = false;
};

bool rn_validate_value_schema(const RNValueSchema &p_schema, RNError &r_error, const String &p_path = "schema");
bool rn_validate_method_schema(const RNMethodSchema &p_schema, RNError &r_error, const String &p_path = "method");
bool rn_validate_event_schema(const RNEventSchema &p_schema, RNError &r_error, const String &p_path = "event");
bool rn_validate_native_value(const Variant &p_value, const RNValueSchema &p_schema, RNError &r_error, const String &p_path);
