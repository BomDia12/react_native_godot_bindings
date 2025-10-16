#include "hermes_runtime_singleton.h"

#include "core/error/error_macros.h"
#include "core/string/print_string.h"

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <memory>
#include <string>
#include <vector>

using facebook::hermes::makeHermesRuntime;

HermesRuntimeSingleton *HermesRuntimeSingleton::singleton = nullptr;

namespace {
	static String _string_from_utf8(const std::string &p_value) {
		return String::utf8(p_value.c_str());
	}

	static std::string _to_utf8(const String &p_value) {
		const CharString utf8 = p_value.utf8();
		return std::string(utf8.get_data(), utf8.length());
	}
}

HermesRuntimeSingleton::HermesRuntimeSingleton() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "HermesRuntimeSingleton is a singleton.");
	singleton = this;

	std::lock_guard<std::mutex> lock(runtime_mutex);
	runtime = makeHermesRuntime();
}

HermesRuntimeSingleton::~HermesRuntimeSingleton() {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	runtime.reset();
	if (singleton == this) {
		singleton = nullptr;
	}
}

HermesRuntimeSingleton *HermesRuntimeSingleton::get_singleton() {
	return singleton;
}

void HermesRuntimeSingleton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("evaluate", "code", "source"), &HermesRuntimeSingleton::evaluate, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("call_function", "function_name", "args"), &HermesRuntimeSingleton::call_function, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("set_global", "name", "value"), &HermesRuntimeSingleton::set_global);
	ClassDB::bind_method(D_METHOD("get_global", "name"), &HermesRuntimeSingleton::get_global);
	ClassDB::bind_method(D_METHOD("reset"), &HermesRuntimeSingleton::reset);
	ClassDB::bind_method(D_METHOD("is_ready"), &HermesRuntimeSingleton::is_ready);
	ClassDB::bind_method(D_METHOD("get_last_error"), &HermesRuntimeSingleton::get_last_error);
}

Variant HermesRuntimeSingleton::evaluate(const String &p_code, const String &p_source) {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return evaluate_locked(p_code, p_source);
}

Variant HermesRuntimeSingleton::call_function(const String &p_function_name, const Array &p_args) {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return call_function_locked(p_function_name, p_args);
}

void HermesRuntimeSingleton::set_global(const String &p_name, const Variant &p_value) {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	set_global_locked(p_name, p_value);
}

Variant HermesRuntimeSingleton::get_global(const String &p_name) {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return get_global_locked(p_name);
}

void HermesRuntimeSingleton::reset() {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	runtime.reset();
	last_error = String();
	ensure_runtime_locked();
}

bool HermesRuntimeSingleton::is_ready() const {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return runtime != nullptr;
}

String HermesRuntimeSingleton::get_last_error() const {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return last_error;
}

void HermesRuntimeSingleton::ensure_runtime_locked() {
	if (!runtime) {
		runtime = makeHermesRuntime();
	}
}

Variant HermesRuntimeSingleton::evaluate_locked(const String &p_code, const String &p_source) {
	ensure_runtime_locked();
	last_error = String();

	std::string code_utf8 = _to_utf8(p_code);
	if (code_utf8.empty()) {
		return Variant();
	}

	std::string source_utf8 = p_source.is_empty() ? std::string("<eval>") : _to_utf8(p_source);
	std::shared_ptr<facebook::jsi::Buffer> buffer = std::make_shared<facebook::jsi::StringBuffer>(code_utf8);

	facebook::jsi::Runtime &rt = *runtime;

	try {
		auto prepared = runtime->prepareJavaScript(buffer, source_utf8);
		facebook::jsi::Value result = runtime->evaluatePreparedJavaScript(prepared);
		return jsi_value_to_variant(rt, result, 0);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
		return Variant();
	}
}

Variant HermesRuntimeSingleton::call_function_locked(const String &p_function_name, const Array &p_args) {
	ensure_runtime_locked();
	last_error = String();

	facebook::jsi::Runtime &rt = *runtime;
	facebook::jsi::Object global = runtime->global();
	std::string func_name = _to_utf8(p_function_name);

	try {
		if (!global.hasProperty(rt, func_name.c_str())) {
			last_error = String("Function not found: ") + p_function_name;
			return Variant();
		}

		facebook::jsi::Value target = global.getProperty(rt, func_name.c_str());
		if (!target.isObject()) {
			last_error = String("Property is not callable: ") + p_function_name;
			return Variant();
		}

		facebook::jsi::Object target_object = target.getObject(rt);
		if (!target_object.isFunction(rt)) {
			last_error = String("Property is not callable: ") + p_function_name;
			return Variant();
		}

		facebook::jsi::Function fn = target_object.getFunction(rt);
		std::vector<facebook::jsi::Value> js_args;
		js_args.reserve(p_args.size());
		for (int i = 0; i < p_args.size(); ++i) {
			js_args.push_back(variant_to_jsi(rt, p_args[i], 0));
		}

		const facebook::jsi::Value *args_ptr = js_args.empty() ? nullptr : js_args.data();
		const size_t arg_count = static_cast<size_t>(js_args.size());
		facebook::jsi::Value result = fn.call(rt, args_ptr, arg_count);
		return jsi_value_to_variant(rt, result, 0);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
		return Variant();
	}
}

void HermesRuntimeSingleton::set_global_locked(const String &p_name, const Variant &p_value) {
	ensure_runtime_locked();
	last_error = String();

	facebook::jsi::Runtime &rt = *runtime;
	facebook::jsi::Object global = runtime->global();
	std::string name_utf8 = _to_utf8(p_name);

	try {
		facebook::jsi::Value js_value = variant_to_jsi(rt, p_value, 0);
		global.setProperty(rt, name_utf8.c_str(), js_value);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
	}
}

Variant HermesRuntimeSingleton::get_global_locked(const String &p_name) {
	ensure_runtime_locked();
	last_error = String();

	facebook::jsi::Runtime &rt = *runtime;
	facebook::jsi::Object global = runtime->global();
	std::string name_utf8 = _to_utf8(p_name);

	try {
		if (!global.hasProperty(rt, name_utf8.c_str())) {
			return Variant();
		}

		facebook::jsi::Value value = global.getProperty(rt, name_utf8.c_str());
		return jsi_value_to_variant(rt, value, 0);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
		return Variant();
	}
}

Variant HermesRuntimeSingleton::jsi_value_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value, int p_depth) {
	if (p_depth > MAX_CONVERSION_DEPTH) {
		return Variant();
	}

	if (p_value.isUndefined() || p_value.isNull()) {
		return Variant();
	}

	if (p_value.isBool()) {
		return p_value.getBool();
	}

	if (p_value.isNumber()) {
		return p_value.getNumber();
	}

	if (p_value.isString()) {
		return _string_from_utf8(p_value.getString(rt).utf8(rt));
	}

	if (p_value.isSymbol()) {
		return _string_from_utf8(p_value.getSymbol(rt).toString(rt));
	}

	if (p_value.isBigInt()) {
		facebook::jsi::String big_str = p_value.getBigInt(rt).toString(rt);
		return _string_from_utf8(big_str.utf8(rt));
	}

	if (p_value.isObject()) {
		facebook::jsi::Object obj = p_value.getObject(rt);
		return object_to_variant(rt, obj, p_depth + 1);
	}

	return Variant();
}

Variant HermesRuntimeSingleton::object_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Object &p_object, int p_depth) {
	if (p_depth > MAX_CONVERSION_DEPTH) {
		return Variant();
	}

	if (p_object.isArray(rt)) {
		facebook::jsi::Array js_array = p_object.asArray(rt);
		const size_t length = js_array.size(rt);
		Array result;
		result.resize(static_cast<int>(length));
		for (size_t i = 0; i < length; ++i) {
			result[static_cast<int>(i)] = jsi_value_to_variant(rt, js_array.getValueAtIndex(rt, i), p_depth + 1);
		}
		return result;
	}

	if (p_object.isFunction(rt)) {
		return String("[Function]");
	}

	if (p_object.isHostObject(rt)) {
		return String("[HostObject]");
	}

	Dictionary dict;
	facebook::jsi::Array names = p_object.getPropertyNames(rt);
	size_t count = names.size(rt);
	if (count > static_cast<size_t>(MAX_OBJECT_PROPERTIES)) {
		count = static_cast<size_t>(MAX_OBJECT_PROPERTIES);
	}

	for (size_t i = 0; i < count; ++i) {
		facebook::jsi::Value key_value = names.getValueAtIndex(rt, i);
		String key_string;
		if (key_value.isString()) {
			key_string = _string_from_utf8(key_value.getString(rt).utf8(rt));
		} else if (key_value.isNumber()) {
			key_string = String::num_real(key_value.getNumber());
		} else {
			continue;
		}

		Variant value = jsi_value_to_variant(rt, p_object.getProperty(rt, key_value), p_depth + 1);
		dict[key_string] = value;
	}

	return dict;
}

facebook::jsi::Value HermesRuntimeSingleton::variant_to_jsi(facebook::jsi::Runtime &rt, const Variant &p_value, int p_depth) {
	if (p_depth > MAX_CONVERSION_DEPTH) {
		return facebook::jsi::Value::undefined();
	}

	switch (p_value.get_type()) {
		case Variant::NIL:
			return facebook::jsi::Value::null();
		case Variant::BOOL:
			return facebook::jsi::Value(bool(p_value));
		case Variant::INT:
			return facebook::jsi::Value(static_cast<double>(int64_t(p_value)));
		case Variant::FLOAT:
			return facebook::jsi::Value(double(p_value));
		case Variant::STRING: {
			String str = p_value;
			std::string utf8 = _to_utf8(str);
			facebook::jsi::String js_str = facebook::jsi::String::createFromUtf8(rt, utf8);
			return facebook::jsi::Value(rt, js_str);
		}
		case Variant::ARRAY: {
			Array array = p_value;
			facebook::jsi::Array js_array(rt, array.size());
			for (int i = 0; i < array.size(); ++i) {
				js_array.setValueAtIndex(rt, i, variant_to_jsi(rt, array[i], p_depth + 1));
			}
			return facebook::jsi::Value(rt, js_array);
		}
		case Variant::DICTIONARY: {
			Dictionary dict = p_value;
			facebook::jsi::Object js_object(rt);
			Array keys = dict.keys();
			for (int i = 0; i < keys.size(); ++i) {
				Variant key_variant = keys[i];
				String key_string = key_variant;
				std::string utf8 = _to_utf8(key_string);
				facebook::jsi::Value js_value = variant_to_jsi(rt, dict[key_variant], p_depth + 1);
				js_object.setProperty(rt, utf8.c_str(), js_value);
			}
			return facebook::jsi::Value(rt, js_object);
		}
		default: {
			String str = p_value;
			std::string utf8 = _to_utf8(str);
			facebook::jsi::String js_str = facebook::jsi::String::createFromUtf8(rt, utf8);
			return facebook::jsi::Value(rt, js_str);
		}
	}
}
