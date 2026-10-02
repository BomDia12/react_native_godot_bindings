#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

struct RNError {
	String code;
	String message;
	String operation;
	String path;
	String module;
	String component;
	int root_tag = 0;
	int tag = 0;
	uint64_t generation = 0;
	uint64_t revision = 0;

	bool is_set() const { return !code.is_empty(); }
	Dictionary to_dictionary() const;
	String describe() const;

	static RNError make(const String &p_code, const String &p_message, const String &p_operation = String(), const String &p_path = String());
};

namespace RNErrorCode {
inline constexpr const char *VALIDATION = "E_VALIDATION";
inline constexpr const char *UNSUPPORTED = "E_UNSUPPORTED";
inline constexpr const char *UNKNOWN_COMPONENT = "E_UNKNOWN_COMPONENT";
inline constexpr const char *UNKNOWN_MODULE = "E_UNKNOWN_MODULE";
inline constexpr const char *DUPLICATE_REGISTRATION = "E_DUPLICATE_REGISTRATION";
inline constexpr const char *STALE_HANDLE = "E_STALE_HANDLE";
inline constexpr const char *OBJECT_GONE = "E_OBJECT_GONE";
inline constexpr const char *CANCELLED = "E_CANCELLED";
inline constexpr const char *SESSION_CLOSED = "E_SESSION_CLOSED";
inline constexpr const char *RUNTIME_RESET = "E_RUNTIME_RESET";
inline constexpr const char *NATIVE = "E_NATIVE";
inline constexpr const char *UNHANDLED_REJECTION = "E_UNHANDLED_REJECTION";
inline constexpr const char *LIMIT = "E_LIMIT";
} // namespace RNErrorCode
