#include "rn_schema.h"

#include "core/math/math_funcs.h"
#include "core/templates/hash_set.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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
			RNValueSchema value_schema = *field.value;
			value_schema.nullable = value_schema.nullable || field.nullable;
			RNError default_error;
			if (!rn_validate_native_value(field.default_value, value_schema, default_error, p_path + "." + String(field.name) + ".default")) {
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
		if (argument.has_default) {
			RNValueSchema value_schema = argument.value;
			value_schema.nullable = value_schema.nullable || argument.nullable;
			if (!rn_validate_native_value(argument.default_value, value_schema, r_error, p_path + ".args." + String(argument.name) + ".default")) {
				return false;
			}
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

static bool validate_native_shape(const Variant &p_value, const RNValueSchema &p_schema, RNError &r_error, const String &p_path) {
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
		case RNValueType::INTEGER: {
			if (p_value.get_type() != Variant::INT) {
				return fail(r_error, "expected integer", p_path);
			}
			constexpr int64_t MAX_SAFE_INTEGER = 9007199254740991;
			const int64_t value = p_value;
			return (value >= -MAX_SAFE_INTEGER && value <= MAX_SAFE_INTEGER) || fail(r_error, "integer is outside Number.MAX_SAFE_INTEGER", p_path);
		}
		case RNValueType::STRING:
			return string_without_nul(p_value) || fail(r_error, "expected a string without embedded NUL", p_path);
		case RNValueType::ARRAY: {
			if (p_value.get_type() != Variant::ARRAY || !p_schema.element) {
				return fail(r_error, "expected array", p_path);
			}
			const Array array = p_value;
			for (int i = 0; i < array.size(); ++i) {
				if (!validate_native_shape(array[i], *p_schema.element, r_error, vformat("%s[%d]", p_path, i))) {
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
				if (!validate_native_shape(value, *field.value, r_error, p_path + "." + String(field.name))) {
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

namespace {
struct NativePayloadBounds {
	std::vector<const void *> ancestors;
	uint64_t values = 0;
	uint64_t bytes = 0;
	bool visit(const Variant &p_value, RNError &r_error, const String &p_path) {
		if (++values > 65536 || ancestors.size() > 32) {
			return fail(r_error, "native payload exceeds structural limits", p_path);
		}
		const auto type = p_value.get_type();
		if (type == Variant::STRING || type == Variant::STRING_NAME) {
			const String value = p_value;
			if (value.contains_char('\0')) {
				return fail(r_error, "native string contains NUL", p_path);
			}
			bytes += value.utf8().length();
		} else if (type == Variant::PACKED_BYTE_ARRAY) {
			bytes += PackedByteArray(p_value).size();
		} else if (type == Variant::ARRAY || type == Variant::DICTIONARY) {
			const void *id = type == Variant::ARRAY ? Array(p_value).id() : Dictionary(p_value).id();
			if (std::find(ancestors.begin(), ancestors.end(), id) != ancestors.end()) {
				return fail(r_error, "cyclic native payload", p_path);
			}
			ancestors.push_back(id);
			struct Guard {
				std::vector<const void *> &ancestors;
				~Guard() { ancestors.pop_back(); }
			} guard{ ancestors };
			if (type == Variant::ARRAY) {
				const Array array = p_value;
				if (array.size() > 4096) {
					return fail(r_error, "native container exceeds entry limit", p_path);
				}
				for (const Variant &value : array) {
					if (!visit(value, r_error, p_path)) {
						return false;
					}
				}
			} else {
				const Dictionary record = p_value;
				if (record.size() > 4096) {
					return fail(r_error, "native container exceeds entry limit", p_path);
				}
				for (const Variant &key : record.keys()) {
					if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) {
						return fail(r_error, "record keys must be strings", p_path);
					}
					if (!visit(key, r_error, p_path) || !visit(record[key], r_error, p_path)) {
						return false;
					}
				}
			}
		} else if (type == Variant::FLOAT && !std::isfinite(double(p_value))) {
			return fail(r_error, "native number must be finite", p_path);
		} else if (type != Variant::NIL && type != Variant::BOOL && type != Variant::INT && type != Variant::FLOAT && type != Variant::COLOR && type != Variant::VECTOR2 && type != Variant::VECTOR3 && type != Variant::RECT2 && type != Variant::TRANSFORM2D) {
			return fail(r_error, "native value is outside the codec contract", p_path);
		} else {
			bytes += 64;
		}
		if (bytes > 16 * 1024 * 1024) {
			r_error = RNError::make(RNErrorCode::LIMIT, "maximum aggregate payload exceeded", "validate", p_path);
			return false;
		}
		return true;
	}
};
} //namespace
bool rn_validate_native_value(const Variant &p_value, const RNValueSchema &p_schema, RNError &r_error, const String &p_path) {
	NativePayloadBounds bounds;
	return bounds.visit(p_value, r_error, p_path) && validate_native_shape(p_value, p_schema, r_error, p_path);
}

namespace {
struct SchemaParser {
	std::vector<const void *> ancestors;
	uint64_t entries = 0;

	bool parse(const Dictionary &p_definition, RNValueSchema &r_schema, RNError &r_error, const String &p_path, bool p_field = false) {
		if (ancestors.size() >= 32 || p_definition.size() > 4096 || entries + p_definition.size() > 65536) {
			return fail(r_error, "schema exceeds structural limits", p_path);
		}
		if (std::find(ancestors.begin(), ancestors.end(), p_definition.id()) != ancestors.end()) {
			return fail(r_error, "cyclic schema", p_path);
		}
		ancestors.push_back(p_definition.id());
		struct Guard {
			std::vector<const void *> &ancestors;
			~Guard() { ancestors.pop_back(); }
		} guard{ ancestors };
		entries += p_definition.size();
		if (p_definition.get("type", Variant()).get_type() != Variant::STRING) {
			return fail(r_error, "type must be a string", p_path);
		}
		const String name = p_definition["type"];
		bool found = false;
		for (int type = int(RNValueType::VOID); type <= int(RNValueType::SESSION); ++type) {
			if (type_name(RNValueType(type)) == name) {
				r_schema = RNValueSchema::value(RNValueType(type));
				found = true;
				break;
			}
		}
		if (!found) {
			return fail(r_error, "unknown schema type", p_path + ".type");
		}
		const Array keys = p_definition.keys();
		for (const Variant &key : keys) {
			if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) {
				return fail(r_error, "schema keys must be strings", p_path);
			}
			const String option = key;
			const bool allowed = option == "type" || option == "nullable" ||
					(r_schema.type == RNValueType::ARRAY && option == "element") ||
					(r_schema.type == RNValueType::RECORD && (option == "fields" || option == "closed")) ||
					(r_schema.type == RNValueType::OBJECT && option == "capability") ||
					(r_schema.type == RNValueType::RECT2 && option == "allow_negative_size") ||
					(p_field && (option == "optional" || option == "default"));
			if (!allowed) {
				return fail(r_error, "unknown or misplaced schema option", p_path + "." + option);
			}
			if ((option == "nullable" || option == "closed" || option == "optional" || option == "allow_negative_size") && p_definition[key].get_type() != Variant::BOOL) {
				return fail(r_error, "schema flag must be boolean", p_path + "." + option);
			}
		}
		r_schema.nullable = p_definition.get("nullable", false);
		r_schema.allow_negative_size = p_definition.get("allow_negative_size", false);
		if (p_definition.has("capability")) {
			if (!string_without_nul(p_definition["capability"]) || String(p_definition["capability"]).is_empty()) {
				return fail(r_error, "capability must be a nonempty string", p_path);
			}
			r_schema.capability = p_definition["capability"];
		}
		if (r_schema.type == RNValueType::ARRAY) {
			if (p_definition.get("element", Variant()).get_type() != Variant::DICTIONARY) {
				return fail(r_error, "array requires an element schema", p_path);
			}
			r_schema.element = std::make_shared<RNValueSchema>();
			if (!parse(p_definition["element"], *r_schema.element, r_error, p_path + ".element")) {
				return false;
			}
		}
		if (r_schema.type == RNValueType::RECORD) {
			if (p_definition.get("fields", Variant()).get_type() != Variant::DICTIONARY) {
				return fail(r_error, "record requires fields", p_path);
			}
			const Dictionary fields = p_definition["fields"];
			if (fields.size() > 4096) {
				return fail(r_error, "too many record fields", p_path);
			}
			r_schema.closed = p_definition.get("closed", true);
			const Array names = fields.keys();
			for (const Variant &field_name : names) {
				if ((field_name.get_type() != Variant::STRING && field_name.get_type() != Variant::STRING_NAME) || String(field_name).is_empty() || fields[field_name].get_type() != Variant::DICTIONARY) {
					return fail(r_error, "invalid record field", p_path);
				}
				const Dictionary definition = fields[field_name];
				RNRecordFieldSchema field;
				field.name = StringName(field_name);
				field.value = std::make_shared<RNValueSchema>();
				if (!parse(definition, *field.value, r_error, p_path + "." + String(field.name), true)) {
					return false;
				}
				field.optional = definition.get("optional", false);
				field.nullable = field.value->nullable;
				field.has_default = definition.has("default");
				if (field.has_default) {
					field.default_value = definition["default"];
					RNError default_error;
					if (!rn_validate_native_value(field.default_value, *field.value, default_error, p_path + ".default")) {
						r_error = default_error;
						return false;
					}
					field.default_value = field.default_value.duplicate(true);
				}
				r_schema.fields.push_back(field);
			}
		}
		return rn_validate_value_schema(r_schema, r_error, p_path);
	}
};
} //namespace

bool rn_parse_value_schema(const Dictionary &p_definition, RNValueSchema &r_schema, RNError &r_error, const String &p_path) {
	SchemaParser parser;
	RNValueSchema compiled;
	if (!parser.parse(p_definition, compiled, r_error, p_path)) {
		return false;
	}
	r_schema = std::move(compiled);
	return true;
}

Variant rn_apply_native_defaults(const Variant &p_value, const RNValueSchema &p_schema) {
	if (p_schema.type == RNValueType::ARRAY && p_value.get_type() == Variant::ARRAY && p_schema.element) {
		Array result;
		for (const Variant &value : Array(p_value)) {
			result.push_back(rn_apply_native_defaults(value, *p_schema.element));
		}
		return result;
	}
	if (p_schema.type == RNValueType::RECORD && p_value.get_type() == Variant::DICTIONARY) {
		Dictionary result = Dictionary(p_value).duplicate(true);
		for (const RNRecordFieldSchema &field : p_schema.fields) {
			if (!result.has(field.name) && field.has_default) {
				result[field.name] = field.default_value.duplicate(true);
			}
			if (result.has(field.name) && field.value) {
				result[field.name] = rn_apply_native_defaults(result[field.name], *field.value);
			}
		}
		return result;
	}
	return p_value.duplicate(true);
}
