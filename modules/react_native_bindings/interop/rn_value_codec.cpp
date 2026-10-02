#include "rn_value_codec.h"

#include "core/templates/hash_set.h"

#include <jsi/jsi.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace {

using namespace facebook;

constexpr double MAX_SAFE_INTEGER = 9007199254740991.0;

String from_utf8(const std::string &p_value) {
	return String::utf8(p_value.data(), int(p_value.size()));
}

std::string to_utf8(const String &p_value) {
	const CharString utf8 = p_value.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

bool fail(RNConversionContext &r_context, RNError &r_error, const String &p_code, const String &p_message, const String &p_path) {
	r_error = RNError::make(p_code, p_message, r_context.operation, p_path);
	return false;
}

bool visit(RNConversionContext &r_context, RNError &r_error, uint32_t p_depth, const String &p_path) {
	if (p_depth > r_context.limits.max_depth) {
		return fail(r_context, r_error, RNErrorCode::LIMIT, vformat("maximum nesting depth is %d", r_context.limits.max_depth), p_path);
	}
	r_context.stats.visited_values++;
	if (r_context.stats.visited_values > r_context.limits.max_visited_values) {
		return fail(r_context, r_error, RNErrorCode::LIMIT, vformat("maximum visited value count is %d", r_context.limits.max_visited_values), p_path);
	}
	return true;
}

bool add_payload(RNConversionContext &r_context, RNError &r_error, uint64_t p_bytes, const String &p_path) {
	if (p_bytes > r_context.limits.max_payload_bytes - MIN(r_context.stats.payload_bytes, r_context.limits.max_payload_bytes)) {
		return fail(r_context, r_error, RNErrorCode::LIMIT, vformat("maximum aggregate payload is %d bytes", r_context.limits.max_payload_bytes), p_path);
	}
	r_context.stats.payload_bytes += p_bytes;
	return true;
}

String child_path(const String &p_path, const String &p_name) {
	return p_path.is_empty() ? p_name : p_path + "." + p_name;
}

bool is_ancestor(jsi::Runtime &p_runtime, const jsi::Object &p_object, const std::vector<jsi::Object> &p_ancestors) {
	for (const jsi::Object &ancestor : p_ancestors) {
		if (jsi::Object::strictEquals(p_runtime, p_object, ancestor)) {
			return true;
		}
	}
	return false;
}

bool is_plain_object(jsi::Runtime &p_runtime, const jsi::Object &p_object) {
	jsi::Object object_constructor = p_runtime.global().getPropertyAsObject(p_runtime, "Object");
	jsi::Function get_prototype = object_constructor.getPropertyAsFunction(p_runtime, "getPrototypeOf");
	jsi::Value prototype_value = get_prototype.call(p_runtime, p_object);
	if (prototype_value.isNull()) {
		return true;
	}
	if (!prototype_value.isObject()) {
		return false;
	}
	jsi::Value base_prototype = object_constructor.getProperty(p_runtime, "prototype");
	return base_prototype.isObject() && jsi::Object::strictEquals(p_runtime, prototype_value.getObject(p_runtime), base_prototype.getObject(p_runtime));
}

bool read_string(jsi::Runtime &p_runtime, const jsi::Value &p_value, String &r_value, RNConversionContext &r_context, RNError &r_error, const String &p_path) {
	if (!p_value.isString()) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected string", p_path);
	}
	const std::string utf8 = p_value.getString(p_runtime).utf8(p_runtime);
	if (utf8.find('\0') != std::string::npos) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "strings cannot contain an embedded NUL", p_path);
	}
	if (!add_payload(r_context, r_error, utf8.size(), p_path)) {
		return false;
	}
	r_value = from_utf8(utf8);
	return true;
}

bool read_finite(jsi::Runtime &p_runtime, const jsi::Object &p_object, const char *p_name, double &r_value, RNConversionContext &r_context, RNError &r_error, const String &p_path) {
	const jsi::Value value = p_object.getProperty(p_runtime, p_name);
	if (!value.isNumber() || !std::isfinite(value.getNumber())) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, vformat("'%s' must be a finite number", p_name), child_path(p_path, p_name));
	}
	r_value = value.getNumber();
	return true;
}

bool wrapper_name(jsi::Runtime &p_runtime, const jsi::Object &p_object, String &r_name, RNConversionContext &r_context, RNError &r_error, const String &p_path) {
	if (!p_object.hasProperty(p_runtime, "$godot")) {
		return false;
	}
	return read_string(p_runtime, p_object.getProperty(p_runtime, "$godot"), r_name, r_context, r_error, child_path(p_path, "$godot"));
}

bool parse_int64(const String &p_text, int64_t &r_value) {
	if (p_text.is_empty() || p_text == "-0" || (p_text.length() > 1 && p_text[0] == '0') || (p_text.length() > 2 && p_text[0] == '-' && p_text[1] == '0')) {
		return false;
	}
	bool negative = p_text[0] == '-';
	int start = negative ? 1 : 0;
	if (start == p_text.length()) {
		return false;
	}
	uint64_t magnitude = 0;
	const uint64_t limit = negative ? uint64_t(std::numeric_limits<int64_t>::max()) + 1 : uint64_t(std::numeric_limits<int64_t>::max());
	for (int i = start; i < p_text.length(); ++i) {
		const char32_t character = p_text[i];
		if (character < '0' || character > '9') {
			return false;
		}
		const uint64_t digit = uint64_t(character - '0');
		if (magnitude > (limit - digit) / 10) {
			return false;
		}
		magnitude = magnitude * 10 + digit;
	}
	if (negative && magnitude == uint64_t(std::numeric_limits<int64_t>::max()) + 1) {
		r_value = std::numeric_limits<int64_t>::min();
	} else {
		r_value = negative ? -int64_t(magnitude) : int64_t(magnitude);
	}
	return true;
}

bool from_js_value(jsi::Runtime &p_runtime, const jsi::Value &p_value, const RNValueSchema &p_schema, RNConversionContext &r_context, uint32_t p_depth, const String &p_path, Variant &r_value, RNError &r_error);

bool read_record(jsi::Runtime &p_runtime, const jsi::Object &p_object, const RNValueSchema &p_schema, RNConversionContext &r_context, uint32_t p_depth, const String &p_path, Variant &r_value, RNError &r_error) {
	if (!is_plain_object(p_runtime, p_object)) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected a plain record", p_path);
	}
	if (is_ancestor(p_runtime, p_object, r_context.ancestors)) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "cyclic values are not supported", p_path);
	}
	jsi::Array names = p_object.getPropertyNames(p_runtime);
	const size_t count = names.size(p_runtime);
	if (count > r_context.limits.max_container_entries) {
		return fail(r_context, r_error, RNErrorCode::LIMIT, vformat("record has more than %d fields", r_context.limits.max_container_entries), p_path);
	}
	r_context.ancestors.push_back(jsi::Value(p_runtime, p_object).getObject(p_runtime));
	Dictionary result;
	r_context.stats.allocated_containers++;
	for (const RNRecordFieldSchema &field : p_schema.fields) {
		const std::string name = to_utf8(String(field.name));
		if (!p_object.hasProperty(p_runtime, name.c_str())) {
			if (field.has_default) {
				result[field.name] = field.default_value;
			} else if (!field.optional) {
				r_context.ancestors.pop_back();
				return fail(r_context, r_error, RNErrorCode::VALIDATION, vformat("missing required field '%s'", field.name), p_path);
			}
			continue;
		}
		const jsi::Value value = p_object.getProperty(p_runtime, name.c_str());
		if (value.isUndefined()) {
			if (field.optional) {
				continue;
			}
			r_context.ancestors.pop_back();
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "required field is undefined", child_path(p_path, String(field.name)));
		}
		if (value.isNull() && field.nullable) {
			result[field.name] = Variant();
			continue;
		}
		Variant decoded;
		if (!from_js_value(p_runtime, value, *field.value, r_context, p_depth + 1, child_path(p_path, String(field.name)), decoded, r_error)) {
			r_context.ancestors.pop_back();
			return false;
		}
		result[field.name] = decoded;
	}
	for (size_t i = 0; i < count; ++i) {
		const jsi::Value key_value = names.getValueAtIndex(p_runtime, i);
		if (!key_value.isString()) {
			continue;
		}
		const std::string key_utf8 = key_value.getString(p_runtime).utf8(p_runtime);
		if (key_utf8.find('\0') != std::string::npos) {
			r_context.ancestors.pop_back();
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "record keys cannot contain an embedded NUL", p_path);
		}
		if (!add_payload(r_context, r_error, key_utf8.size(), p_path)) {
			r_context.ancestors.pop_back();
			return false;
		}
		const String key = from_utf8(key_utf8);
		bool declared = false;
		for (const RNRecordFieldSchema &field : p_schema.fields) {
			declared = declared || field.name == StringName(key);
		}
		if (!declared && (key == "$godot" || p_schema.closed)) {
			r_context.ancestors.pop_back();
			return fail(r_context, r_error, RNErrorCode::VALIDATION, vformat("unlisted field '%s'", key), child_path(p_path, key));
		}
		if (!declared) {
			RNValueSchema dynamic = RNValueSchema::value(RNValueType::DYNAMIC);
			Variant decoded;
			if (!from_js_value(p_runtime, p_object.getProperty(p_runtime, to_utf8(key).c_str()), dynamic, r_context, p_depth + 1, child_path(p_path, key), decoded, r_error)) {
				r_context.ancestors.pop_back();
				return false;
			}
			result[key] = decoded;
		}
	}
	r_context.ancestors.pop_back();
	r_value = result;
	return true;
}

bool from_js_object(jsi::Runtime &p_runtime, const jsi::Object &p_object, const RNValueSchema &p_schema, RNConversionContext &r_context, uint32_t p_depth, const String &p_path, Variant &r_value, RNError &r_error) {
	if (p_object.isFunction(p_runtime)) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "functions use dedicated callback members and cannot be data", p_path);
	}
	if (p_object.isHostObject(p_runtime)) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "HostObjects cannot be data", p_path);
	}
	if (p_schema.type == RNValueType::BYTES || p_schema.type == RNValueType::DYNAMIC) {
		if (p_object.isTypedArray(p_runtime) && !p_object.isUint8Array(p_runtime)) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "only Uint8Array is supported", p_path);
		}
		if (p_object.isUint8Array(p_runtime)) {
			jsi::Uint8Array typed = p_object.asUint8Array(p_runtime);
			jsi::ArrayBuffer buffer = typed.buffer(p_runtime);
			if (buffer.detached(p_runtime)) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "Uint8Array buffer is detached", p_path);
			}
			const size_t length = typed.byteLength(p_runtime);
			if (!add_payload(r_context, r_error, length, p_path)) {
				return false;
			}
			PackedByteArray bytes;
			bytes.resize(int(length));
			if (length > 0) {
				std::memcpy(bytes.ptrw(), buffer.data(p_runtime) + typed.byteOffset(p_runtime), length);
			}
			r_context.stats.copied_binary_bytes += length;
			r_value = bytes;
			return true;
		}
		if (p_schema.type == RNValueType::BYTES) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected Uint8Array", p_path);
		}
	}
	if (p_schema.type == RNValueType::ARRAY || (p_schema.type == RNValueType::DYNAMIC && p_object.isArray(p_runtime))) {
		if (!p_object.isArray(p_runtime)) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected array", p_path);
		}
		if (is_ancestor(p_runtime, p_object, r_context.ancestors)) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "cyclic values are not supported", p_path);
		}
		jsi::Array source = p_object.asArray(p_runtime);
		const size_t count = source.size(p_runtime);
		if (count > r_context.limits.max_container_entries) {
			return fail(r_context, r_error, RNErrorCode::LIMIT, vformat("array has more than %d elements", r_context.limits.max_container_entries), p_path);
		}
		r_context.ancestors.push_back(jsi::Value(p_runtime, p_object).getObject(p_runtime));
		Array result;
		result.resize(int(count));
		r_context.stats.allocated_containers++;
		const RNValueSchema dynamic = RNValueSchema::value(RNValueType::DYNAMIC);
		const RNValueSchema &element = p_schema.element ? *p_schema.element : dynamic;
		for (size_t i = 0; i < count; ++i) {
			jsi::Value value = source.getValueAtIndex(p_runtime, i);
			if (value.isUndefined()) {
				r_context.ancestors.pop_back();
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "array entries cannot be absent or undefined", vformat("%s[%d]", p_path, i));
			}
			Variant decoded;
			if (!from_js_value(p_runtime, value, element, r_context, p_depth + 1, vformat("%s[%d]", p_path, i), decoded, r_error)) {
				r_context.ancestors.pop_back();
				return false;
			}
			result[int(i)] = decoded;
		}
		r_context.ancestors.pop_back();
		r_value = result;
		return true;
	}
	String wrapper;
	RNError wrapper_error;
	const bool has_wrapper = wrapper_name(p_runtime, p_object, wrapper, r_context, wrapper_error, p_path);
	if (wrapper_error.is_set()) {
		r_error = wrapper_error;
		return false;
	}
	auto require_wrapper = [&](const char *p_expected) {
		return has_wrapper && wrapper == p_expected;
	};
	if (p_schema.type == RNValueType::INT64 && require_wrapper("int64")) {
		String text;
		if (!read_string(p_runtime, p_object.getProperty(p_runtime, "value"), text, r_context, r_error, child_path(p_path, "value"))) {
			return false;
		}
		int64_t value = 0;
		if (!parse_int64(text, value)) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "int64 value must be a canonical signed decimal in range", child_path(p_path, "value"));
		}
		r_value = value;
		return true;
	}
	if (p_schema.type == RNValueType::COLOR && require_wrapper("Color")) {
		double r, g, b, a;
		if (!read_finite(p_runtime, p_object, "r", r, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "g", g, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "b", b, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "a", a, r_context, r_error, p_path)) {
			return false;
		}
		if (r < 0 || r > 1 || g < 0 || g > 1 || b < 0 || b > 1 || a < 0 || a > 1) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "Color channels must be in the range 0-1", p_path);
		}
		r_value = Color(float(r), float(g), float(b), float(a));
		return true;
	}
	if ((p_schema.type == RNValueType::VECTOR2 && require_wrapper("Vector2")) || (p_schema.type == RNValueType::VECTOR3 && require_wrapper("Vector3"))) {
		double x, y;
		if (!read_finite(p_runtime, p_object, "x", x, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "y", y, r_context, r_error, p_path)) {
			return false;
		}
		if (p_schema.type == RNValueType::VECTOR2) {
			r_value = Vector2(x, y);
			return true;
		}
		double z;
		if (!read_finite(p_runtime, p_object, "z", z, r_context, r_error, p_path)) {
			return false;
		}
		r_value = Vector3(x, y, z);
		return true;
	}
	if (p_schema.type == RNValueType::RECT2 && require_wrapper("Rect2")) {
		double x, y, width, height;
		if (!read_finite(p_runtime, p_object, "x", x, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "y", y, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "width", width, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "height", height, r_context, r_error, p_path)) {
			return false;
		}
		if (!p_schema.allow_negative_size && (width < 0 || height < 0)) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "Rect2 sizes cannot be negative", p_path);
		}
		r_value = Rect2(x, y, width, height);
		return true;
	}
	if (p_schema.type == RNValueType::TRANSFORM2D && require_wrapper("Transform2D")) {
		double xx, xy, yx, yy, ox, oy;
		if (!read_finite(p_runtime, p_object, "xx", xx, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "xy", xy, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "yx", yx, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "yy", yy, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "ox", ox, r_context, r_error, p_path) || !read_finite(p_runtime, p_object, "oy", oy, r_context, r_error, p_path)) {
			return false;
		}
		r_value = Transform2D(Vector2(xx, xy), Vector2(yx, yy), Vector2(ox, oy));
		return true;
	}
	if ((p_schema.type == RNValueType::OBJECT && require_wrapper("Object")) || (p_schema.type == RNValueType::SESSION && require_wrapper("Session"))) {
		String handle;
		if (!read_string(p_runtime, p_object.getProperty(p_runtime, "handle"), handle, r_context, r_error, child_path(p_path, "handle")) || handle.is_empty()) {
			return false;
		}
		r_value = handle;
		return true;
	}
	if (p_schema.type == RNValueType::DYNAMIC && has_wrapper) {
		RNValueType inferred = RNValueType::DYNAMIC;
		if (wrapper == "int64") {
			inferred = RNValueType::INT64;
		} else if (wrapper == "Color") {
			inferred = RNValueType::COLOR;
		} else if (wrapper == "Vector2") {
			inferred = RNValueType::VECTOR2;
		} else if (wrapper == "Vector3") {
			inferred = RNValueType::VECTOR3;
		} else if (wrapper == "Rect2") {
			inferred = RNValueType::RECT2;
		} else if (wrapper == "Transform2D") {
			inferred = RNValueType::TRANSFORM2D;
		} else if (wrapper == "Object") {
			inferred = RNValueType::OBJECT;
		} else if (wrapper == "Session") {
			inferred = RNValueType::SESSION;
		}
		if (inferred == RNValueType::DYNAMIC) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, vformat("unknown Godot wrapper '%s'", wrapper), p_path);
		}
		return from_js_object(p_runtime, p_object, RNValueSchema::value(inferred), r_context, p_depth, p_path, r_value, r_error);
	}
	if (p_schema.type == RNValueType::RECORD || p_schema.type == RNValueType::DYNAMIC) {
		if (has_wrapper) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "typed wrapper cannot masquerade as a record", p_path);
		}
		RNValueSchema record_schema = p_schema;
		if (record_schema.type == RNValueType::DYNAMIC) {
			record_schema = RNValueSchema::record({}, false);
		}
		return read_record(p_runtime, p_object, record_schema, r_context, p_depth, p_path, r_value, r_error);
	}
	return fail(r_context, r_error, RNErrorCode::VALIDATION, vformat("value does not match required typed wrapper"), p_path);
}

bool from_js_value(jsi::Runtime &p_runtime, const jsi::Value &p_value, const RNValueSchema &p_schema, RNConversionContext &r_context, uint32_t p_depth, const String &p_path, Variant &r_value, RNError &r_error) {
	if (!visit(r_context, r_error, p_depth, p_path)) {
		return false;
	}
	if (p_value.isUndefined()) {
		if (p_schema.type == RNValueType::VOID) {
			r_value = Variant();
			return true;
		}
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "undefined is only valid for omitted optional values or void results", p_path);
	}
	if (p_value.isNull()) {
		if (p_schema.nullable || p_schema.type == RNValueType::NULL_VALUE || p_schema.type == RNValueType::DYNAMIC) {
			r_value = Variant();
			return true;
		}
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "null is not allowed", p_path);
	}
	if (p_schema.type == RNValueType::VOID) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, "void result must be undefined", p_path);
	}
	if (p_value.isBool()) {
		if (p_schema.type != RNValueType::BOOL && p_schema.type != RNValueType::DYNAMIC) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected another value type, got boolean", p_path);
		}
		r_value = p_value.getBool();
		return true;
	}
	if (p_value.isNumber()) {
		const double number = p_value.getNumber();
		if (!std::isfinite(number)) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "number must be finite", p_path);
		}
		if (p_schema.type == RNValueType::INTEGER) {
			if (std::trunc(number) != number || std::abs(number) > MAX_SAFE_INTEGER) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "integer must be integral and within Number.MAX_SAFE_INTEGER", p_path);
			}
			r_value = int64_t(number);
			return true;
		}
		if (p_schema.type != RNValueType::FLOAT && p_schema.type != RNValueType::DYNAMIC) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected another value type, got number", p_path);
		}
		r_value = number;
		return true;
	}
	if (p_value.isString()) {
		if (p_schema.type != RNValueType::STRING && p_schema.type != RNValueType::DYNAMIC) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected another value type, got string", p_path);
		}
		String value;
		if (!read_string(p_runtime, p_value, value, r_context, r_error, p_path)) {
			return false;
		}
		r_value = value;
		return true;
	}
	if (p_value.isSymbol() || p_value.isBigInt()) {
		return fail(r_context, r_error, RNErrorCode::VALIDATION, p_value.isSymbol() ? "symbols cannot be data" : "BigInt is unsupported; use the int64 wrapper", p_path);
	}
	if (p_value.isObject()) {
		return from_js_object(p_runtime, p_value.getObject(p_runtime), p_schema, r_context, p_depth, p_path, r_value, r_error);
	}
	return fail(r_context, r_error, RNErrorCode::VALIDATION, "unsupported JavaScript value", p_path);
}

void set_string(jsi::Runtime &p_runtime, jsi::Object &r_object, const char *p_name, const String &p_value) {
	r_object.setProperty(p_runtime, p_name, jsi::String::createFromUtf8(p_runtime, to_utf8(p_value)));
}

bool to_js_value(jsi::Runtime &p_runtime, const Variant &p_value, const RNValueSchema &p_schema, RNConversionContext &r_context, uint32_t p_depth, const String &p_path, jsi::Value &r_value, RNError &r_error) {
	if (!visit(r_context, r_error, p_depth, p_path)) {
		return false;
	}
	if (p_schema.type == RNValueType::VOID) {
		if (p_value.get_type() != Variant::NIL) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "void result contains a value", p_path);
		}
		r_value = jsi::Value::undefined();
		return true;
	}
	if (p_value.get_type() == Variant::NIL) {
		if (!p_schema.nullable && p_schema.type != RNValueType::DYNAMIC && p_schema.type != RNValueType::NULL_VALUE) {
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "null is not allowed", p_path);
		}
		r_value = jsi::Value::null();
		return true;
	}
	const RNValueType type = p_schema.type == RNValueType::DYNAMIC ? [&]() {
		switch (p_value.get_type()) {
			case Variant::BOOL:
				return RNValueType::BOOL;
			case Variant::FLOAT:
				return RNValueType::FLOAT;
			case Variant::INT:
				return std::abs(double(int64_t(p_value))) <= MAX_SAFE_INTEGER ? RNValueType::INTEGER : RNValueType::INT64;
			case Variant::STRING:
				return RNValueType::STRING;
			case Variant::ARRAY:
				return RNValueType::ARRAY;
			case Variant::DICTIONARY:
				return RNValueType::RECORD;
			case Variant::PACKED_BYTE_ARRAY:
				return RNValueType::BYTES;
			case Variant::COLOR:
				return RNValueType::COLOR;
			case Variant::VECTOR2:
				return RNValueType::VECTOR2;
			case Variant::VECTOR3:
				return RNValueType::VECTOR3;
			case Variant::RECT2:
				return RNValueType::RECT2;
			case Variant::TRANSFORM2D:
				return RNValueType::TRANSFORM2D;
			default:
				return RNValueType::VOID;
		}
	}()
																   : p_schema.type;
	switch (type) {
		case RNValueType::BOOL:
			if (p_value.get_type() != Variant::BOOL) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected boolean", p_path);
			}
			r_value = jsi::Value(bool(p_value));
			return true;
		case RNValueType::FLOAT:
			if ((p_value.get_type() != Variant::FLOAT && p_value.get_type() != Variant::INT) || !std::isfinite(double(p_value))) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected finite number", p_path);
			}
			r_value = jsi::Value(double(p_value));
			return true;
		case RNValueType::INTEGER: {
			if (p_value.get_type() != Variant::INT || std::abs(double(int64_t(p_value))) > MAX_SAFE_INTEGER) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "integer is outside Number.MAX_SAFE_INTEGER", p_path);
			}
			r_value = jsi::Value(double(int64_t(p_value)));
			return true;
		}
		case RNValueType::STRING: {
			if (p_value.get_type() != Variant::STRING) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected string", p_path);
			}
			const std::string utf8 = to_utf8(String(p_value));
			if (utf8.find('\0') != std::string::npos || !add_payload(r_context, r_error, utf8.size(), p_path)) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "strings cannot contain embedded NUL", p_path);
			}
			r_value = jsi::String::createFromUtf8(p_runtime, utf8);
			return true;
		}
		case RNValueType::ARRAY: {
			if (p_value.get_type() != Variant::ARRAY) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected array", p_path);
			}
			const Array source = p_value;
			if (source.size() > int(r_context.limits.max_container_entries)) {
				return fail(r_context, r_error, RNErrorCode::LIMIT, "array is too large", p_path);
			}
			jsi::Array result(p_runtime, source.size());
			r_context.stats.allocated_containers++;
			const RNValueSchema dynamic = RNValueSchema::value(RNValueType::DYNAMIC);
			const RNValueSchema &element = p_schema.element ? *p_schema.element : dynamic;
			for (int i = 0; i < source.size(); ++i) {
				jsi::Value value;
				if (!to_js_value(p_runtime, source[i], element, r_context, p_depth + 1, vformat("%s[%d]", p_path, i), value, r_error)) {
					return false;
				}
				result.setValueAtIndex(p_runtime, i, std::move(value));
			}
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::RECORD: {
			if (p_value.get_type() != Variant::DICTIONARY) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected record", p_path);
			}
			const Dictionary source = p_value;
			if (source.size() > int(r_context.limits.max_container_entries)) {
				return fail(r_context, r_error, RNErrorCode::LIMIT, "record is too large", p_path);
			}
			if (!rn_validate_native_value(source, p_schema.type == RNValueType::DYNAMIC ? RNValueSchema::record({}, false) : p_schema, r_error, p_path)) {
				return false;
			}
			jsi::Object result(p_runtime);
			r_context.stats.allocated_containers++;
			const Array keys = source.keys();
			HashSet<String> converted_keys;
			for (int i = 0; i < keys.size(); ++i) {
				if (keys[i].get_type() != Variant::STRING && keys[i].get_type() != Variant::STRING_NAME) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "record keys must be strings", p_path);
				}
				const String key = keys[i].get_type() == Variant::STRING ? String(keys[i]) : String(StringName(keys[i]));
				if (converted_keys.has(key)) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "record keys must remain unique strings", p_path);
				}
				converted_keys.insert(key);
				const std::string key_utf8 = to_utf8(key);
				if (key_utf8.find('\0') != std::string::npos) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "record keys cannot contain an embedded NUL", p_path);
				}
				if (!add_payload(r_context, r_error, key_utf8.size(), p_path)) {
					return false;
				}
				RNValueSchema field_schema = RNValueSchema::value(RNValueType::DYNAMIC);
				for (const RNRecordFieldSchema &field : p_schema.fields) {
					if (field.name == StringName(key)) {
						field_schema = *field.value;
						field_schema.nullable = field_schema.nullable || field.nullable;
						break;
					}
				}
				jsi::Value value;
				if (!to_js_value(p_runtime, source[keys[i]], field_schema, r_context, p_depth + 1, child_path(p_path, key), value, r_error)) {
					return false;
				}
				result.setProperty(p_runtime, key_utf8.c_str(), std::move(value));
			}
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::BYTES: {
			if (p_value.get_type() != Variant::PACKED_BYTE_ARRAY) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected PackedByteArray", p_path);
			}
			const PackedByteArray bytes = p_value;
			if (!add_payload(r_context, r_error, bytes.size(), p_path)) {
				return false;
			}
			jsi::Uint8Array result(p_runtime, bytes.size());
			if (!bytes.is_empty()) {
				std::memcpy(result.buffer(p_runtime).data(p_runtime) + result.byteOffset(p_runtime), bytes.ptr(), bytes.size());
			}
			r_context.stats.copied_binary_bytes += bytes.size();
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::INT64: {
			if (p_value.get_type() != Variant::INT) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected int64", p_path);
			}
			jsi::Object result(p_runtime);
			set_string(p_runtime, result, "$godot", "int64");
			set_string(p_runtime, result, "value", String::num_int64(int64_t(p_value)));
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::COLOR: {
			if (p_value.get_type() != Variant::COLOR) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected Color", p_path);
			}
			const Color value = p_value;
			if (!std::isfinite(value.r) || !std::isfinite(value.g) || !std::isfinite(value.b) || !std::isfinite(value.a) || value.r < 0 || value.r > 1 || value.g < 0 || value.g > 1 || value.b < 0 || value.b > 1 || value.a < 0 || value.a > 1) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "Color channels must be in the range 0-1", p_path);
			}
			jsi::Object result(p_runtime);
			set_string(p_runtime, result, "$godot", "Color");
			result.setProperty(p_runtime, "r", value.r);
			result.setProperty(p_runtime, "g", value.g);
			result.setProperty(p_runtime, "b", value.b);
			result.setProperty(p_runtime, "a", value.a);
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::VECTOR2:
		case RNValueType::VECTOR3:
		case RNValueType::RECT2:
		case RNValueType::TRANSFORM2D: {
			jsi::Object result(p_runtime);
			if (type == RNValueType::VECTOR2 && p_value.get_type() == Variant::VECTOR2) {
				const Vector2 v = p_value;
				if (!v.is_finite()) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "Vector2 components must be finite", p_path);
				}
				set_string(p_runtime, result, "$godot", "Vector2");
				result.setProperty(p_runtime, "x", v.x);
				result.setProperty(p_runtime, "y", v.y);
			} else if (type == RNValueType::VECTOR3 && p_value.get_type() == Variant::VECTOR3) {
				const Vector3 v = p_value;
				if (!v.is_finite()) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "Vector3 components must be finite", p_path);
				}
				set_string(p_runtime, result, "$godot", "Vector3");
				result.setProperty(p_runtime, "x", v.x);
				result.setProperty(p_runtime, "y", v.y);
				result.setProperty(p_runtime, "z", v.z);
			} else if (type == RNValueType::RECT2 && p_value.get_type() == Variant::RECT2) {
				const Rect2 v = p_value;
				if (!v.position.is_finite() || !v.size.is_finite() || (!p_schema.allow_negative_size && (v.size.x < 0 || v.size.y < 0))) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "Rect2 components must be finite and sizes must satisfy the schema", p_path);
				}
				set_string(p_runtime, result, "$godot", "Rect2");
				result.setProperty(p_runtime, "x", v.position.x);
				result.setProperty(p_runtime, "y", v.position.y);
				result.setProperty(p_runtime, "width", v.size.x);
				result.setProperty(p_runtime, "height", v.size.y);
			} else if (type == RNValueType::TRANSFORM2D && p_value.get_type() == Variant::TRANSFORM2D) {
				const Transform2D v = p_value;
				if (!v.columns[0].is_finite() || !v.columns[1].is_finite() || !v.columns[2].is_finite()) {
					return fail(r_context, r_error, RNErrorCode::VALIDATION, "Transform2D components must be finite", p_path);
				}
				set_string(p_runtime, result, "$godot", "Transform2D");
				result.setProperty(p_runtime, "xx", v.columns[0].x);
				result.setProperty(p_runtime, "xy", v.columns[0].y);
				result.setProperty(p_runtime, "yx", v.columns[1].x);
				result.setProperty(p_runtime, "yy", v.columns[1].y);
				result.setProperty(p_runtime, "ox", v.columns[2].x);
				result.setProperty(p_runtime, "oy", v.columns[2].y);
			} else {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "typed native value does not match schema", p_path);
			}
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::OBJECT:
		case RNValueType::SESSION: {
			if (p_value.get_type() != Variant::STRING || String(p_value).is_empty()) {
				return fail(r_context, r_error, RNErrorCode::VALIDATION, "expected opaque handle token", p_path);
			}
			jsi::Object result(p_runtime);
			set_string(p_runtime, result, "$godot", type == RNValueType::OBJECT ? "Object" : "Session");
			set_string(p_runtime, result, "handle", p_value);
			r_value = jsi::Value(p_runtime, result);
			return true;
		}
		case RNValueType::DYNAMIC:
		case RNValueType::NULL_VALUE:
		case RNValueType::VOID:
			return fail(r_context, r_error, RNErrorCode::VALIDATION, "unsupported native Variant kind", p_path);
	}
	return fail(r_context, r_error, RNErrorCode::VALIDATION, "unsupported native Variant kind", p_path);
}

} // namespace

RNDecodedValue RNValueCodec::from_js(facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &p_value, const RNValueSchema &p_schema, const String &p_operation, const String &p_path) {
	RNConversionContext context;
	context.operation = p_operation;
	context.path = p_path;
	RNDecodedValue result;
	from_js_value(p_runtime, p_value, p_schema, context, 0, p_path, result.value, result.error);
	result.stats = context.stats;
	return result;
}

bool RNValueCodec::to_js(facebook::jsi::Runtime &p_runtime, const Variant &p_value, const RNValueSchema &p_schema, facebook::jsi::Value &r_value, RNError &r_error, RNConversionStats *r_stats, const String &p_operation, const String &p_path) {
	RNConversionContext context;
	context.operation = p_operation;
	context.path = p_path;
	const bool converted = to_js_value(p_runtime, p_value, p_schema, context, 0, p_path, r_value, r_error);
	if (r_stats) {
		*r_stats = context.stats;
	}
	return converted;
}
