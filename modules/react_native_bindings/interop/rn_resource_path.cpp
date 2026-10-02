#include "rn_resource_path.h"

#include "core/variant/variant.h"

bool rn_normalize_local_resource_path(const String &p_path, String &r_normalized, RNError &r_error, const String &p_operation) {
	r_normalized = String();
	if (p_path.contains_char('\0')) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "local resource path contains an embedded NUL", p_operation, "path");
		return false;
	}
	if (p_path.contains("\\")) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "local resource paths use forward slashes", p_operation, "path");
		return false;
	}
	String scheme;
	String relative;
	if (p_path.begins_with("res://")) {
		scheme = "res://";
		relative = p_path.substr(6);
	} else if (p_path.begins_with("user://")) {
		scheme = "user://";
		relative = p_path.substr(7);
	} else {
		r_error = RNError::make(RNErrorCode::VALIDATION, "local resource path must use the exact res:// or user:// scheme", p_operation, "path");
		return false;
	}
	if (relative.is_empty() || relative.begins_with("/")) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "local resource path is empty or absolute", p_operation, "path");
		return false;
	}
	const PackedStringArray parts = relative.split("/", false);
	PackedStringArray normalized;
	for (const String &part : parts) {
		if (part == "." || part.is_empty()) {
			continue;
		}
		if (part == "..") {
			r_error = RNError::make(RNErrorCode::VALIDATION, "local resource path must not contain a '..' segment", p_operation, "path");
			return false;
		}
		normalized.push_back(part);
	}
	if (normalized.is_empty()) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "local resource path has no filename", p_operation, "path");
		return false;
	}
	r_normalized = scheme + String("/").join(normalized);
	return true;
}
