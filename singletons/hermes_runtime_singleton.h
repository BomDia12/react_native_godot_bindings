#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"

#include <memory>
#include <mutex>

namespace facebook {
	namespace hermes {
		class HermesRuntime;
	} // namespace hermes

	namespace jsi {
		class Runtime;
		class Value;
		class Object;
	} // namespace jsi
} // namespace facebook

class HermesRuntimeSingleton : public Object {
	GDCLASS(HermesRuntimeSingleton, Object);

	static constexpr int MAX_CONVERSION_DEPTH = 6;
	static constexpr int MAX_OBJECT_PROPERTIES = 128;

	static HermesRuntimeSingleton *singleton;

	std::unique_ptr<facebook::hermes::HermesRuntime> runtime;
	mutable std::mutex runtime_mutex;
	String last_error;
	Callable import_resolver;

	Variant evaluate_locked(const String &code, const String &source);
	Variant call_function_locked(const String &function_name, const Array &args);
	void set_global_locked(const String &name, const Variant &value);
	Variant get_global_locked(const String &name);
	Variant jsi_value_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Value &value, int depth);
	Variant object_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Object &object, int depth);
	facebook::jsi::Value variant_to_jsi(facebook::jsi::Runtime &rt, const Variant &value, int depth);
	void ensure_runtime_locked();
	void install_import_function_locked();
	facebook::jsi::Value handle_import_module(facebook::jsi::Runtime &rt, const facebook::jsi::Value *args, size_t argc);
 	Variant filesystem_import_resolver(const String &path);

protected:
	static void _bind_methods();

public:
	HermesRuntimeSingleton();
	~HermesRuntimeSingleton() override;

	static HermesRuntimeSingleton *get_singleton();

	Variant evaluate(const String &code, const String &source = String());
	Variant call_function(const String &function_name, const Array &args = Array());
	void set_global(const String &name, const Variant &value);
	Variant get_global(const String &name);
	void reset();
	bool is_ready() const;
	String get_last_error() const;
	void set_import_resolver(const Callable &resolver);
	Callable get_import_resolver() const;
 	void use_filesystem_import_resolver();
	facebook::jsi::Object run_file(const String &p_file, const String &p_path, Error &err);
};
