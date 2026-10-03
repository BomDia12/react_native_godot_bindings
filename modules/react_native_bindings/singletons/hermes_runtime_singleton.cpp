#include "hermes_runtime_singleton.h"

#include "../fabric/fabric_ui_manager.h"
#include "../interop/rn_resource_path.h"
#include "../interop/rn_value_codec.h"
#include "../native_modules/rn_native_module_registry.h"
#include "../runtime/rn_execution_scope.h"
#include "../runtime/rn_runtime_scheduler.h"
#include "hermes_runtime_lifecycle.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/os/thread.h"
#include "core/string/print_string.h"
#include "core/string/string_name.h"

#include <hermes/hermes.h>
#include <jsi/instrumentation.h>
#include <jsi/jsi.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using facebook::hermes::makeHermesRuntime;

HermesRuntimeSingleton *HermesRuntimeSingleton::singleton = nullptr;

namespace {
// The length is passed so the conversion does not re-scan for a terminator it already
// knows. It does not preserve an embedded NUL: String::append_utf8() stops at the first
// zero byte whatever length it is given, so a JS string containing one is cut short here.
static String _string_from_utf8(const std::string &p_value) {
	return String::utf8(p_value.c_str(), int(p_value.length()));
}

static std::string _to_utf8(const String &p_value) {
	const CharString utf8 = p_value.utf8();
	return std::string(utf8.get_data(), utf8.length());
}
static constexpr const char *IMPORT_FUNCTION_NAME = "importModule";

static Dictionary _import_failure(const String &p_message, const String &p_path) {
	Dictionary result;
	result[SNAME("error")] = p_message;
	result[SNAME("path")] = p_path;
	return result;
}
} //namespace

HermesRuntimeSingleton::HermesRuntimeSingleton() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "HermesRuntimeSingleton is a singleton.");
	singleton = this;

	std::lock_guard<std::mutex> lock(runtime_mutex);
	runtime = makeHermesRuntime(::hermes::vm::RuntimeConfig::Builder().withMicrotaskQueue(true).build());
	import_resolver = callable_mp(this, &HermesRuntimeSingleton::filesystem_import_resolver);
	install_import_function_locked();
	install_runtime_functions_locked();
}

HermesRuntimeSingleton::~HermesRuntimeSingleton() {
	std::lock_guard<std::mutex> lock(runtime_mutex);
	run_pre_reset_hooks_locked();
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
	ClassDB::bind_method(D_METHOD("has_global_function", "name"), &HermesRuntimeSingleton::has_global_function);
	ClassDB::bind_method(D_METHOD("reset"), &HermesRuntimeSingleton::reset);
#ifdef DEBUG_ENABLED
	ClassDB::bind_method(D_METHOD("collect_garbage"), &HermesRuntimeSingleton::collect_garbage);
#endif
	ClassDB::bind_method(D_METHOD("get_runtime_generation"), &HermesRuntimeSingleton::get_runtime_generation);
	ClassDB::bind_method(D_METHOD("is_ready"), &HermesRuntimeSingleton::is_ready);
	ClassDB::bind_method(D_METHOD("get_last_error"), &HermesRuntimeSingleton::get_last_error);
	ClassDB::bind_method(D_METHOD("set_import_resolver", "resolver"), &HermesRuntimeSingleton::set_import_resolver);
	ClassDB::bind_method(D_METHOD("get_import_resolver"), &HermesRuntimeSingleton::get_import_resolver);
	ClassDB::bind_method(D_METHOD("use_filesystem_import_resolver"), &HermesRuntimeSingleton::use_filesystem_import_resolver);
}

Variant HermesRuntimeSingleton::evaluate(const String &p_code, const String &p_source) {
	ERR_FAIL_COND_V_MSG(!require_main_thread("evaluate"), Variant(), "HermesRuntime.evaluate() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return evaluate_locked(p_code, p_source);
}

Variant HermesRuntimeSingleton::call_function(const String &p_function_name, const Array &p_args) {
	ERR_FAIL_COND_V_MSG(!require_main_thread("call_function"), Variant(), "HermesRuntime.call_function() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return call_function_locked(p_function_name, p_args);
}

void HermesRuntimeSingleton::set_global(const String &p_name, const Variant &p_value) {
	ERR_FAIL_COND_MSG(!require_main_thread("set_global"), "HermesRuntime.set_global() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	set_global_locked(p_name, p_value);
}

Variant HermesRuntimeSingleton::get_global(const String &p_name) {
	ERR_FAIL_COND_V_MSG(!require_main_thread("get_global"), Variant(), "HermesRuntime.get_global() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return get_global_locked(p_name);
}

bool HermesRuntimeSingleton::has_global_function(const String &p_name) {
	ERR_FAIL_COND_V_MSG(!require_main_thread("has_global_function"), false, "HermesRuntime.has_global_function() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	ensure_runtime_locked();
	last_error = String();
	try {
		const std::string name = _to_utf8(p_name);
		facebook::jsi::Object global = runtime->global();
		if (!global.hasProperty(*runtime, name.c_str())) {
			return false;
		}
		facebook::jsi::Value value = global.getProperty(*runtime, name.c_str());
		return value.isObject() && value.getObject(*runtime).isFunction(*runtime);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		return false;
	}
}

void HermesRuntimeSingleton::collect_garbage() {
	ERR_FAIL_COND_MSG(!require_main_thread("collect_garbage"), "Garbage collection must run on the main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	if (runtime) {
		runtime->instrumentation().collectGarbage("Godot diagnostic");
	}
}

void HermesRuntimeSingleton::reset() {
	ERR_FAIL_COND_MSG(!require_main_thread("reset"), "HermesRuntime.reset() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	run_pre_reset_hooks_locked();
	runtime.reset();
	++runtime_generation;
	last_error = String();
	ensure_runtime_locked();
}

uint64_t HermesRuntimeSingleton::get_runtime_generation() const {
	ERR_FAIL_COND_V_MSG(!require_main_thread("get_runtime_generation"), 0, "HermesRuntime.get_runtime_generation() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return runtime_generation;
}

void HermesRuntimeSingleton::dispatch_queued_events(const std::shared_ptr<FabricUIManager> &p_ui_manager) {
	ERR_FAIL_COND_MSG(!require_main_thread("dispatch_queued_events"), "HermesRuntime.dispatch_queued_events() must run on Godot's main thread.");
	if (!p_ui_manager) {
		return;
	}

	std::lock_guard<std::mutex> lock(runtime_mutex);
	if (!runtime) {
		return;
	}
	p_ui_manager->dispatch_queued_events_locked(*runtime, runtime_generation);
	run_microtask_checkpoint_locked();
}

size_t HermesRuntimeSingleton::dispatch_native_module_deliveries(const std::shared_ptr<RNNativeModuleRegistry> &p_registry) {
	ERR_FAIL_COND_V_MSG(!require_main_thread("dispatch_native_module_deliveries"), 0, "HermesRuntime.dispatch_native_module_deliveries() must run on Godot's main thread.");
	if (!p_registry) {
		return 0;
	}
	std::lock_guard<std::mutex> lock(runtime_mutex);
	if (!runtime) {
		return 0;
	}
	const size_t delivered = p_registry->deliver_locked(*runtime, runtime_generation, [this] { run_microtask_checkpoint_locked(); });
	run_microtask_checkpoint_locked();
	return delivered;
}

void HermesRuntimeSingleton::dispatch_scheduler(const std::shared_ptr<RNRuntimeScheduler> &p_scheduler, bool p_idle, bool p_visual_frame, double p_available_ms, size_t p_native_delivered) {
	ERR_FAIL_COND_MSG(!require_main_thread("dispatch_scheduler"), "Scheduler must run on the main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	if (!runtime || !p_scheduler) {
		return;
	}
	auto checkpoint = [this] { run_microtask_checkpoint_locked(); };
	if (p_idle) {
		p_scheduler->process_idle_locked(*runtime, checkpoint, p_available_ms);
	} else {
		p_scheduler->process_frame_locked(*runtime, p_visual_frame, checkpoint, p_native_delivered);
	}
}

bool HermesRuntimeSingleton::is_ready() const {
	ERR_FAIL_COND_V_MSG(!require_main_thread("is_ready"), false, "HermesRuntime.is_ready() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return runtime != nullptr;
}

String HermesRuntimeSingleton::get_last_error() const {
	ERR_FAIL_COND_V_MSG(!require_main_thread("get_last_error"), String(), "HermesRuntime.get_last_error() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return last_error;
}

void HermesRuntimeSingleton::ensure_runtime_locked() {
	if (!runtime) {
		runtime = makeHermesRuntime(::hermes::vm::RuntimeConfig::Builder().withMicrotaskQueue(true).build());
		if (import_resolver.is_null()) {
			import_resolver = callable_mp(this, &HermesRuntimeSingleton::filesystem_import_resolver);
		}
		install_import_function_locked();
		install_runtime_functions_locked();
		install_host_objects_locked();
	}
}

void HermesRuntimeSingleton::install_host_object(const String &p_name, std::shared_ptr<facebook::jsi::HostObject> p_object) {
	ERR_FAIL_COND_MSG(!require_main_thread("install_host_object"), "HermesRuntime.install_host_object() must run on Godot's main thread.");
	ERR_FAIL_COND_MSG(p_name.is_empty(), "HermesRuntime: host object name is empty.");
	ERR_FAIL_COND_MSG(p_object == nullptr, "HermesRuntime: host object is null.");

	std::lock_guard<std::mutex> lock(runtime_mutex);
	host_objects[p_name] = p_object;
	std::shared_ptr<HermesRuntimeLifecycle> lifecycle = std::dynamic_pointer_cast<HermesRuntimeLifecycle>(p_object);
	if (lifecycle && !is_lifecycle_registered_locked(lifecycle)) {
		lifecycle_objects.push_back(lifecycle);
	}
	ensure_runtime_locked();

	facebook::jsi::Runtime &rt = *runtime;
	try {
		facebook::jsi::Object js_object = facebook::jsi::Object::createFromHostObject(rt, p_object);
		rt.global().setProperty(rt, _to_utf8(p_name).c_str(), js_object);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
	}
}

void HermesRuntimeSingleton::uninstall_host_object(const String &p_name) {
	ERR_FAIL_COND_MSG(!require_main_thread("uninstall_host_object"), "HermesRuntime.uninstall_host_object() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	host_objects.erase(p_name);
	if (!runtime) {
		return;
	}
	try {
		runtime->global().setProperty(*runtime, _to_utf8(p_name).c_str(), facebook::jsi::Value::undefined());
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
	}
}

bool HermesRuntimeSingleton::require_main_thread(const char *p_method) const {
	(void)p_method;
	if (Thread::is_main_thread()) {
		return true;
	}
	return false;
}

bool HermesRuntimeSingleton::is_lifecycle_registered_locked(const std::shared_ptr<HermesRuntimeLifecycle> &p_lifecycle) const {
	for (const std::weak_ptr<HermesRuntimeLifecycle> &entry : lifecycle_objects) {
		if (entry.lock() == p_lifecycle) {
			return true;
		}
	}
	return false;
}

void HermesRuntimeSingleton::run_pre_reset_hooks_locked() {
	if (!runtime) {
		return;
	}

	auto it = lifecycle_objects.begin();
	while (it != lifecycle_objects.end()) {
		if (std::shared_ptr<HermesRuntimeLifecycle> lifecycle = it->lock()) {
			lifecycle->before_runtime_reset_locked(*runtime, runtime_generation);
			++it;
		} else {
			it = lifecycle_objects.erase(it);
		}
	}
}

void HermesRuntimeSingleton::install_host_objects_locked() {
	if (!runtime) {
		return;
	}

	facebook::jsi::Runtime &rt = *runtime;
	for (const KeyValue<String, std::shared_ptr<facebook::jsi::HostObject>> &entry : host_objects) {
		if (entry.value == nullptr) {
			continue;
		}
		try {
			facebook::jsi::Object js_object = facebook::jsi::Object::createFromHostObject(rt, entry.value);
			rt.global().setProperty(rt, _to_utf8(entry.key).c_str(), js_object);
		} catch (const facebook::jsi::JSIException &p_error) {
			last_error = _string_from_utf8(std::string(p_error.what()));
			WARN_PRINT(last_error);
		}
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
		facebook::jsi::Value result = runtime->evaluateJavaScript(buffer, source_utf8);
		Variant converted = jsi_value_to_variant(rt, result);
		run_microtask_checkpoint_locked();
		return converted;
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
			js_args.push_back(variant_to_jsi(rt, p_args[i]));
		}

		const facebook::jsi::Value *args_ptr = js_args.empty() ? nullptr : js_args.data();
		const size_t arg_count = static_cast<size_t>(js_args.size());
		facebook::jsi::Value result = fn.call(rt, args_ptr, arg_count);
		Variant converted = jsi_value_to_variant(rt, result);
		run_microtask_checkpoint_locked();
		return converted;
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
		facebook::jsi::Value js_value = variant_to_jsi(rt, p_value);
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
		return jsi_value_to_variant(rt, value);
	} catch (const facebook::jsi::JSIException &p_error) {
		last_error = _string_from_utf8(std::string(p_error.what()));
		WARN_PRINT(last_error);
		return Variant();
	}
}

Variant HermesRuntimeSingleton::jsi_value_to_variant(facebook::jsi::Runtime &rt, const facebook::jsi::Value &p_value) {
	if (p_value.isUndefined()) {
		return Variant();
	}
	RNDecodedValue converted = RNValueCodec::from_js(rt, p_value, RNValueSchema::value(RNValueType::DYNAMIC), "HermesRuntime.result");
	if (!converted.ok()) {
		last_error = converted.error.describe();
		throw facebook::jsi::JSError(rt, _to_utf8(last_error));
	}
	return converted.value;
}

facebook::jsi::Value HermesRuntimeSingleton::variant_to_jsi(facebook::jsi::Runtime &rt, const Variant &p_value) {
	facebook::jsi::Value converted;
	RNError error;
	if (!RNValueCodec::to_js(rt, p_value, RNValueSchema::value(RNValueType::DYNAMIC), converted, error, nullptr, "HermesRuntime.argument")) {
		last_error = error.describe();
		throw facebook::jsi::JSError(rt, _to_utf8(last_error));
	}
	return converted;
}

void HermesRuntimeSingleton::run_microtask_checkpoint_locked() {
	if (!runtime || microtask_checkpoint_active) {
		return;
	}
	RNExecutionScope clear_scope({});
	microtask_checkpoint_active = true;
	try {
		runtime->drainMicrotasks();
	} catch (...) {
		microtask_checkpoint_active = false;
		throw;
	}
	microtask_checkpoint_active = false;
}

void HermesRuntimeSingleton::install_runtime_functions_locked() {
	if (!runtime) {
		return;
	}
	facebook::jsi::Runtime &rt = *runtime;
	facebook::jsi::Function queue_microtask = facebook::jsi::Function::createFromHostFunction(
			rt, facebook::jsi::PropNameID::forAscii(rt, "__godotQueueMicrotask"), 1,
			[](facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &, const facebook::jsi::Value *p_args, size_t p_count) {
				if (p_count != 1 || !p_args[0].isObject() || !p_args[0].getObject(p_runtime).isFunction(p_runtime)) {
					throw facebook::jsi::JSError(p_runtime, "__godotQueueMicrotask expects one function.");
				}
				p_runtime.queueMicrotask(p_args[0].getObject(p_runtime).getFunction(p_runtime));
				return facebook::jsi::Value::undefined();
			});
	rt.global().setProperty(rt, "__godotQueueMicrotask", std::move(queue_microtask));
	facebook::jsi::Function report_runtime_error = facebook::jsi::Function::createFromHostFunction(
			rt, facebook::jsi::PropNameID::forAscii(rt, "__godotReportRuntimeError"), 1,
			[](facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &, const facebook::jsi::Value *p_args, size_t p_count) {
				if (p_count != 1 || !p_args[0].isObject()) {
					throw facebook::jsi::JSError(p_runtime, "__godotReportRuntimeError expects one structured error object.");
				}
				facebook::jsi::Object error = p_args[0].getObject(p_runtime);
				auto read_string = [&](const char *p_name, bool p_required) {
					facebook::jsi::Value value = error.getProperty(p_runtime, p_name);
					if (!value.isString()) {
						if (p_required) {
							throw facebook::jsi::JSError(p_runtime, std::string("runtime error field '") + p_name + "' must be a string.");
						}
						return String();
					}
					const std::string text = value.getString(p_runtime).utf8(p_runtime);
					if (text.find('\0') != std::string::npos) {
						throw facebook::jsi::JSError(p_runtime, "runtime error fields cannot contain NUL.");
					}
					return _string_from_utf8(text);
				};
				const String code = read_string("code", true);
				const String message = read_string("message", true);
				const String operation = read_string("operation", true);
				facebook::jsi::Value rejection = error.getProperty(p_runtime, "rejectionId");
				String suffix;
				if (rejection.isNumber() && std::isfinite(rejection.getNumber()) && std::trunc(rejection.getNumber()) == rejection.getNumber()) {
					suffix = vformat(" rejection=%d", int64_t(rejection.getNumber()));
				}
				print_line(vformat("RN_GODOT_COMPAT: %s [%s] %s%s", code, operation, message, suffix));
				return facebook::jsi::Value::undefined();
			});
	rt.global().setProperty(rt, "__godotReportRuntimeError", std::move(report_runtime_error));
}

void HermesRuntimeSingleton::install_import_function_locked() {
	if (!runtime) {
		return;
	}

	facebook::jsi::Runtime &rt = *runtime;
	facebook::jsi::Function host_import = facebook::jsi::Function::createFromHostFunction(
			rt,
			facebook::jsi::PropNameID::forAscii(rt, IMPORT_FUNCTION_NAME),
			1,
			[this](facebook::jsi::Runtime &rt_inner, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t argc) -> facebook::jsi::Value {
				return handle_import_module(rt_inner, args, argc);
			});

	facebook::jsi::Object global = runtime->global();
	global.setProperty(rt, IMPORT_FUNCTION_NAME, std::move(host_import));
}

facebook::jsi::Value HermesRuntimeSingleton::handle_import_module(facebook::jsi::Runtime &rt, const facebook::jsi::Value *p_args, size_t p_argc) {
	if (!import_resolver.is_valid()) {
		throw facebook::jsi::JSError(rt, std::string("HermesRuntime: import resolver is not set."));
	}

	if (p_argc < 1 || p_args == nullptr || !p_args[0].isString()) {
		throw facebook::jsi::JSError(rt, std::string("HermesRuntime: importModule expects a module specifier string."));
	}

	String module_specifier = _string_from_utf8(p_args[0].getString(rt).utf8(rt));
	Variant spec_variant = module_specifier;
	const Variant *call_args[1] = { &spec_variant };
	Callable::CallError call_error;
	Variant resolver_result;
	import_resolver.callp(call_args, 1, resolver_result, call_error);

	if (call_error.error != Callable::CallError::CALL_OK) {
		String err_msg = String("HermesRuntime: import resolver failed (error code ") + String::num_int64(call_error.error) + ").";
		throw facebook::jsi::JSError(rt, _to_utf8(err_msg));
	}

	String module_code;
	String module_source_name = module_specifier;

	switch (resolver_result.get_type()) {
		case Variant::STRING:
			module_code = resolver_result;
			break;
		case Variant::DICTIONARY: {
			Dictionary dict = resolver_result;
			if (dict.has(SNAME("error"))) {
				String err_msg = dict[SNAME("error")];
				throw facebook::jsi::JSError(rt, _to_utf8(err_msg));
			}
			if (dict.has(SNAME("code"))) {
				module_code = dict[SNAME("code")];
			}
			if (dict.has(SNAME("path"))) {
				module_source_name = dict[SNAME("path")];
			}
			break;
		}
		default:
			break;
	}

	if (module_code.is_empty()) {
		String err_msg = last_error.is_empty() ? String("HermesRuntime: import resolver did not return source code.") : last_error;
		throw facebook::jsi::JSError(rt, _to_utf8(err_msg));
	}

	const std::string code_utf8 = _to_utf8(module_code);
	const std::string source_utf8 = _to_utf8(module_source_name.is_empty() ? module_specifier : module_source_name);
	last_error = String();

	// The factory is returned to the caller and never stored on the global object: a
	// specifier is arbitrary JS-supplied text, so naming a global after it would let
	// importModule("Object") overwrite a core global.
	const std::string wrapped_source = "(function(){\n" + code_utf8 + "\n})";
	auto wrapped_buffer = std::make_shared<facebook::jsi::StringBuffer>(wrapped_source);

	facebook::jsi::Value factory_value = rt.evaluateJavaScript(wrapped_buffer, source_utf8);
	if (!factory_value.isObject() || !factory_value.getObject(rt).isFunction(rt)) {
		throw facebook::jsi::JSError(rt, std::string("HermesRuntime: module resolver did not provide a callable factory."));
	}

	return facebook::jsi::Value(rt, factory_value.getObject(rt).getFunction(rt));
}

void HermesRuntimeSingleton::set_import_resolver(const Callable &p_resolver) {
	ERR_FAIL_COND_MSG(!require_main_thread("set_import_resolver"), "HermesRuntime.set_import_resolver() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	import_resolver = p_resolver;
	install_import_function_locked();
}

Callable HermesRuntimeSingleton::get_import_resolver() const {
	ERR_FAIL_COND_V_MSG(!require_main_thread("get_import_resolver"), Callable(), "HermesRuntime.get_import_resolver() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	return import_resolver;
}

void HermesRuntimeSingleton::use_filesystem_import_resolver() {
	ERR_FAIL_COND_MSG(!require_main_thread("use_filesystem_import_resolver"), "HermesRuntime.use_filesystem_import_resolver() must run on Godot's main thread.");
	std::lock_guard<std::mutex> lock(runtime_mutex);
	import_resolver = callable_mp(this, &HermesRuntimeSingleton::filesystem_import_resolver);
	install_import_function_locked();
}

// The specifier reaching this resolver is arbitrary text from the JS bundle, so it is
// confined to the project and user data directories. Without the prefix check,
// importModule("/etc/passwd") would hand the file back to JavaScript.
Variant HermesRuntimeSingleton::filesystem_import_resolver(const String &p_path) {
	String normalized;
	RNError path_error;
	if (!rn_normalize_local_resource_path(p_path, normalized, path_error, "HermesRuntime.importModule")) {
		last_error = path_error.describe();
		return _import_failure(last_error, p_path);
	}

	Error err = OK;
	String code = FileAccess::get_file_as_string(normalized, &err);
	if (err != OK) {
		last_error = vformat("HermesRuntime: failed to read module '%s': %s", normalized, err);
		return _import_failure(last_error, normalized);
	}

	last_error = String();
	Dictionary result;
	result[SNAME("code")] = code;
	result[SNAME("path")] = normalized;
	return result;
}
