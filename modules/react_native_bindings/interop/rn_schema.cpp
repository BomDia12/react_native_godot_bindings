#include "rn_schema.h"

#include "core/math/math_funcs.h"
#include "core/templates/hash_set.h"

#include <cmath>
#include <string>

namespace {

String type_name(RNValueType p_type) {
	switch (p_type) {
		case RNValueType::VOID:
			return "void";
		case RNValueType::DYNAMIC:
			return "dynamic";
		case RNValueType::NULL_VALUE:
			return "null";
		case RNValueType::BOOL:
			return "boolean";
		case RNValueType::FLOAT:
			return "number";
		case RNValueType::INTEGER:
			return "integer";
		case RNValueType::STRING:
			return "string";
		case RNValueType::ARRAY:
			return "array";
		case RNValueType::RECORD:
			return "record";
		case RNValueType::BYTES:
			return "Uint8Array";
		case RNValueType::INT64:
			return "int64";
		case RNValueType::COLOR:
			return "Color";
		case RNValueType::VECTOR2:
			return "Vector2";
		case RNValueType::VECTOR3:
			return "Vector3";
		case RNValueType::RECT2:
			return "Rect2";
		case RNValueType::TRANSFORM2D:
			return "Transform2D";
		case RNValueType::OBJECT:
			return "Object";
		case RNValueType::SESSION:
			return "Session";
	}
	return "unknown";
}

bool fail(RNError &r_error, const String &p_message, const String &p_path) {
	r_error = RNError::make(RNErrorCode::VALIDATION, p_message, "schema", p_path);
	return false;
}

bool finite_number(const Variant &p_value) {
	return (p_value.get_type() == Variant::FLOAT || p_value.get_type() == Variant::INT) && std::isfinite(double(p_value));
}

bool string_without_nul(const Variant &p_value) {
	if (p_value.get_type() != Variant::STRING) {
		return false;
	}
	const CharString utf8 = String(p_value).utf8();
	return std::string(utf8.get_data(), utf8.length()).find('\0') == std::string::npos;
}

} // namespace

RNValueSchema RNValueSchema::value(RNValueType p_type) {
	RNValueSchema schema;
	schema.type = p_type;
	return schema;
}

RNValueSchema RNValueSchema::array(const RNValueSchema &p_element) {
	RNValueSchema schema = value(RNValueType::ARRAY);
	schema.element = std::make_shared<RNValueSchema>(p_element);
	return schema;
}

RNValueSchema RNValueSchema::record(const Vector<RNRecordFieldSchema> &p_fields, bool p_closed) {
	RNValueSchema schema = value(RNValueType::RECORD);
	schema.fields = p_fields;
	schema.closed = p_closed;
	return schema;
}

Dictionary RNValueSchema::to_metadata() const {
	Dictionary result;
	result[SNAME("type")] = type_name(type);
	result[SNAME("nullable")] = nullable;
	if (type == RNValueType::ARRAY && element) {
		result[SNAME("element")] = element->to_metadata();
	}
	if (type == RNValueType::RECORD) {
		Dictionary metadata_fields;
		for (const RNRecordFieldSchema &field : fields) {
			Dictionary metadata = field.value ? field.value->to_metadata() : Dictionary();
			metadata[SNAME("optional")] = field.optional;
			metadata[SNAME("nullable")] = field.nullable;
			metadata_fields[field.name] = metadata;
		}
		result[SNAME("fields")] = metadata_fields;
		result[SNAME("closed")] = closed;
	}
	if (!capability.is_empty()) {
		result[SNAME("capability")] = capability;
	}
	return result;
}

bool rn_validate_value_schema(const RNValueSchema &p_schema, RNError &r_error, const String &p_path) {
	if (p_schema.type == RNValueType::ARRAY) {
		if (!p_schema.element) {
			return fail(r_error, "array schema is missing its element type", p_path);
		}
		return rn_validate_value_schema(*p_schema.element, r_error, p_path + ".element");
	}
	if (p_schema.type != RNValueType::RECORD) {
		return true;
	}
	HashSet<StringName> names;
	for (const RNRecordFieldSchema &field : p_schema.fields) {
		if (field.name.is_empty()) {
			return fail(r_error, "record field name is empty", p_path);
		}
		if (names.has(field.name)) {
			return fail(r_error, vformat("duplicate record field '%s'", field.name), p_path);
		}
		names.insert(field.name);
		if (!field.value) {
			return fail(r_error, vformat("record field '%s' has no value schema", field.name), p_path);
		}
		if (!rn_validate_value_schema(*field.value, r_error, p_path + "." + String(field.name))) {
			return false;
		}
		if (field.has_default) {
			RNError default_error;
			if (!rn_validate_native_value(field.default_value, *field.value, default_error, p_path + "." + String(field.name) + ".default")) {
				r_error = default_error;
				return false;
			}
		}
	}
	return true;
}

bool rn_validate_method_schema(const RNMethodSchema &p_schema, RNError &r_error, const String &p_path) {
	if (p_schema.name.is_empty()) {
		return fail(r_error, "method name is empty", p_path);
	}
	HashSet<StringName> names;
	for (const RNArgumentSchema &argument : p_schema.arguments) {
		if (argument.name.is_empty() || names.has(argument.name)) {
			return fail(r_error, vformat("invalid or duplicate argument '%s'", argument.name), p_path);
		}
		names.insert(argument.name);
		if (!rn_validate_value_schema(argument.value, r_error, p_path + ".args." + String(argument.name))) {
			return false;
		}
		if (argument.has_default && !rn_validate_native_value(argument.default_value, argument.value, r_error, p_path + ".args." + String(argument.name) + ".default")) {
			return false;
		}
	}
	return rn_validate_value_schema(p_schema.result, r_error, p_path + ".result");
}

bool rn_validate_event_schema(const RNEventSchema &p_schema, RNError &r_error, const String &p_path) {
	if (p_schema.name.is_empty() || p_schema.subscription_name.is_empty()) {
		return fail(r_error, "event and subscription names must be non-empty", p_path);
	}
	return rn_validate_value_schema(p_schema.payload, r_error, p_path + ".payload");
}

bool rn_validate_native_value(const Variant &p_value, const RNValueSchema &p_schema, RNError &r_error, const String &p_path) {
	if (p_value.get_type() == Variant::NIL) {
		if (p_schema.nullable || p_schema.type == RNValueType::NULL_VALUE || p_schema.type == RNValueType::DYNAMIC || p_schema.type == RNValueType::VOID) {
			return true;
		}
		return fail(r_error, "value is null", p_path);
	}
	switch (p_schema.type) {
		case RNValueType::VOID:
			return fail(r_error, "void schema cannot carry a value", p_path);
		case RNValueType::DYNAMIC:
			return true;
		case RNValueType::NULL_VALUE:
			return fail(r_error, "expected null", p_path);
		case RNValueType::BOOL:
			return p_value.get_type() == Variant::BOOL || fail(r_error, "expected boolean", p_path);
		case RNValueType::FLOAT:
			return finite_number(p_value) || fail(r_error, "expected finite number", p_path);
		case RNValueType::INTEGER:
			return p_value.get_type() == Variant::INT || fail(r_error, "expected integer", p_path);
		case RNValueType::STRING:
			return string_without_nul(p_value) || fail(r_error, "expected a string without embedded NUL", p_path);
		case RNValueType::ARRAY: {
			if (p_value.get_type() != Variant::ARRAY || !p_schema.element) {
				return fail(r_error, "expected array", p_path);
			}
			const Array array = p_value;
			for (int i = 0; i < array.size(); ++i) {
				if (!rn_validate_native_value(array[i], *p_schema.element, r_error, vformat("%s[%d]", p_path, i))) {
					return false;
				}
			}
			return true;
		}
		case RNValueType::RECORD: {
			if (p_value.get_type() != Variant::DICTIONARY) {
				return fail(r_error, "expected record", p_path);
			}
			const Dictionary record = p_value;
			for (const RNRecordFieldSchema &field : p_schema.fields) {
				if (!record.has(field.name)) {
					if (!field.optional && !field.has_default) {
						return fail(r_error, vformat("missing required field '%s'", field.name), p_path);
					}
					continue;
				}
				const Variant value = record[field.name];
				if (value.get_type() == Variant::NIL && field.nullable) {
					continue;
				}
				if (!rn_validate_native_value(value, *field.value, r_error, p_path + "." + String(field.name))) {
					return false;
				}
			}
			if (p_schema.closed) {
				const Array keys = record.keys();
				for (int i = 0; i < keys.size(); ++i) {
					bool found = false;
					for (const RNRecordFieldSchema &field : p_schema.fields) {
						found = found || StringName(keys[i]) == field.name;
					}
					if (!found) {
						return fail(r_error, vformat("unlisted field '%s'", keys[i]), p_path);
					}
				}
			}
			return true;
		}
		case RNValueType::BYTES:
			return p_value.get_type() == Variant::PACKED_BYTE_ARRAY || fail(r_error, "expected PackedByteArray", p_path);
		case RNValueType::INT64:
			return p_value.get_type() == Variant::INT || fail(r_error, "expected int64", p_path);
		case RNValueType::COLOR: {
			if (p_value.get_type() != Variant::COLOR) {
				return fail(r_error, "expected Color", p_path);
			}
			const Color color = p_value;
			return (std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) && std::isfinite(color.a) && color.r >= 0 && color.r <= 1 && color.g >= 0 && color.g <= 1 && color.b >= 0 && color.b <= 1 && color.a >= 0 && color.a <= 1) || fail(r_error, "Color channels must be finite and in the range 0-1", p_path);
		}
		case RNValueType::VECTOR2:
			return (p_value.get_type() == Variant::VECTOR2 && Vector2(p_value).is_finite()) || fail(r_error, "expected a finite Vector2", p_path);
		case RNValueType::VECTOR3:
			return (p_value.get_type() == Variant::VECTOR3 && Vector3(p_value).is_finite()) || fail(r_error, "expected a finite Vector3", p_path);
		case RNValueType::RECT2: {
			if (p_value.get_type() != Variant::RECT2) {
				return fail(r_error, "expected Rect2", p_path);
			}
			const Rect2 rect = p_value;
			return (rect.position.is_finite() && rect.size.is_finite() && (p_schema.allow_negative_size || (rect.size.x >= 0 && rect.size.y >= 0))) || fail(r_error, "Rect2 components must be finite and sizes must satisfy the schema", p_path);
		}
		case RNValueType::TRANSFORM2D: {
			if (p_value.get_type() != Variant::TRANSFORM2D) {
				return fail(r_error, "expected Transform2D", p_path);
			}
			const Transform2D transform = p_value;
			return (transform.columns[0].is_finite() && transform.columns[1].is_finite() && transform.columns[2].is_finite()) || fail(r_error, "Transform2D components must be finite", p_path);
		}
		case RNValueType::OBJECT:
		case RNValueType::SESSION:
			return p_value.get_type() == Variant::STRING || fail(r_error, "expected opaque handle token", p_path);
	}
	return fail(r_error, "unsupported schema type", p_path);
}
