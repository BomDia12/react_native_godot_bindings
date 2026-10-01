#include "rn_error.h"

#include "core/string/string_name.h"
#include "core/variant/variant.h"

Dictionary RNError::to_dictionary() const {
	Dictionary result;
	result[SNAME("code")] = code;
	result[SNAME("message")] = message;
	result[SNAME("operation")] = operation;
	result[SNAME("path")] = path;
	if (!module.is_empty()) {
		result[SNAME("module")] = module;
	}
	if (!component.is_empty()) {
		result[SNAME("component")] = component;
	}
	if (root_tag != 0) {
		result[SNAME("rootTag")] = root_tag;
	}
	if (tag != 0) {
		result[SNAME("tag")] = tag;
	}
	if (generation != 0) {
		result[SNAME("generation")] = int64_t(generation);
	}
	if (revision != 0) {
		result[SNAME("revision")] = int64_t(revision);
	}
	return result;
}

String RNError::describe() const {
	String result = code;
	if (!operation.is_empty()) {
		result += " [" + operation + "]";
	}
	if (!path.is_empty()) {
		result += " at " + path;
	}
	if (!message.is_empty()) {
		result += ": " + message;
	}
	return result;
}

RNError RNError::make(const String &p_code, const String &p_message, const String &p_operation, const String &p_path) {
	RNError error;
	error.code = p_code;
	error.message = p_message;
	error.operation = p_operation;
	error.path = p_path;
	return error;
}
