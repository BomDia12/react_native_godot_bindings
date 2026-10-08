#include "rn_native_module_registry.h"

#include "../interop/rn_value_codec.h"
#include "../runtime/react_native_runtime_coordinator.h"
#include "rn_blob_service.h"
#include "rn_http_service.h"

#include "core/string/print_string.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <variant>
#include <vector>

namespace {

std::string to_utf8(const String &p_value) {
	const CharString encoded = p_value.utf8();
	return std::string(encoded.get_data(), encoded.length());
}

String from_utf8(const std::string &p_value) {
	return String::utf8(p_value.data(), int(p_value.size()));
}

struct PromiseCapture {
	std::unique_ptr<facebook::jsi::Function> resolve;
	std::unique_ptr<facebook::jsi::Function> reject;
};

bool is_reserved_member(const StringName &p_name) {
	return p_name == "__godotStartAsync" || p_name == "__godotNativeModuleBrand" || p_name == "__godotSchema" || p_name == "__proto__" || p_name == "constructor";
}

int require_root_tag(facebook::jsi::Runtime &p_runtime, const facebook::jsi::Value &p_value) {
	if (!p_value.isNumber()) {
		throw facebook::jsi::JSError(p_runtime, "openSession expects a root tag");
	}
	const double value = p_value.getNumber();
	constexpr double MAX_SAFE_INTEGER = 9007199254740991.0;
	if (!std::isfinite(value) || std::trunc(value) != value || std::abs(value) > MAX_SAFE_INTEGER || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
		throw facebook::jsi::JSError(p_runtime, "openSession expects a finite safe integer in the native int range");
	}
	return int(value);
}

} // namespace

RNNativeModuleRegistry::RNNativeModuleRegistry(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) :
		state(p_state),
		objects(p_state) {
}

const RNModuleDefinition *RNNativeModuleRegistry::definition(const String &p_name) const {
	return definitions.getptr(p_name);
}

const RNMethodSchema *RNNativeModuleRegistry::method_schema(const RNModuleDefinition &p_definition, const StringName &p_method) const {
	for (const RNMethodSchema &method : p_definition.methods) {
		if (method.name == p_method) {
			return &method;
		}
	}
	return nullptr;
}

const RNEventSchema *RNNativeModuleRegistry::event_schema(const RNModuleDefinition &p_definition, const StringName &p_subscription) const {
	for (const RNEventSchema &event : p_definition.events) {
		if (event.subscription_name == p_subscription) {
			return &event;
		}
	}
	return nullptr;
}

Dictionary RNNativeModuleRegistry::module_metadata(const RNModuleDefinition &p_definition) const {
	Dictionary metadata;
	metadata["name"] = p_definition.name;
	Dictionary methods;
	for (const RNMethodSchema &method : p_definition.methods) {
		Dictionary method_metadata;
		method_metadata["mode"] = method.mode == RNCallMode::SYNC ? "sync" : "async";
		method_metadata["requiresSession"] = method.requires_session;
		Array arguments;
		for (const RNArgumentSchema &argument : method.arguments) {
			Dictionary argument_metadata = argument.value.to_metadata();
			argument_metadata["name"] = String(argument.name);
			argument_metadata["optional"] = argument.optional;
			argument_metadata["nullable"] = argument.nullable;
			argument_metadata["hasDefault"] = argument.has_default;
			if (argument.has_default) {
				argument_metadata["default"] = argument.default_value;
			}
			arguments.push_back(argument_metadata);
		}
		method_metadata["arguments"] = arguments;
		method_metadata["result"] = method.result.to_metadata();
		methods[String(method.name)] = method_metadata;
	}
	metadata["methods"] = methods;
	Dictionary events_metadata;
	for (const RNEventSchema &event : p_definition.events) {
		Dictionary event_metadata;
		event_metadata["name"] = String(event.name);
		event_metadata["subscription"] = String(event.subscription_name);
		event_metadata["requiresSession"] = event.requires_session;
		event_metadata["payload"] = event.payload.to_metadata();
		events_metadata[String(event.subscription_name)] = event_metadata;
	}
	metadata["events"] = events_metadata;
	return metadata;
}

Dictionary RNNativeModuleRegistry::get_module_metadata(const String &p_name) const {
	const RNModuleDefinition *module = definition(p_name);
	return module ? module_metadata(*module) : Dictionary();
}

bool RNNativeModuleRegistry::register_module(const RNModuleDefinition &p_definition, RNError &r_error) {
	if (definitions_frozen) {
		r_error = RNError::make(RNErrorCode::DUPLICATE_REGISTRATION, "native module definitions are frozen", "registerModule");
		return false;
	}
	if (p_definition.name.is_empty() || !p_definition.factory || definitions.has(p_definition.name) || p_definition.name == "NativeDOMCxx" || p_definition.name == "__proto__" || p_definition.name == "constructor") {
		r_error = RNError::make(RNErrorCode::DUPLICATE_REGISTRATION, vformat("native module '%s' is invalid, reserved, or already registered", p_definition.name), "registerModule");
		return false;
	}
	HashSet<StringName> members;
	for (const RNMethodSchema &method : p_definition.methods) {
		if (members.has(method.name) || is_reserved_member(method.name) || !rn_validate_method_schema(method, r_error, p_definition.name + "." + String(method.name))) {
			if (!r_error.is_set()) {
				r_error = RNError::make(RNErrorCode::DUPLICATE_REGISTRATION, vformat("duplicate or reserved module member '%s'", method.name), "registerModule");
			}
			return false;
		}
		members.insert(method.name);
		if (method.requires_session && (method.arguments.is_empty() || method.arguments[0].name != "session" || method.arguments[0].value.type != RNValueType::SESSION)) {
			r_error = RNError::make(RNErrorCode::VALIDATION, vformat("session-scoped method '%s' must declare session first", method.name), "registerModule");
			return false;
		}
	}
	HashSet<StringName> event_names;
	for (const RNEventSchema &event : p_definition.events) {
		if (event_names.has(event.name) || members.has(event.subscription_name) || is_reserved_member(event.subscription_name) || !rn_validate_event_schema(event, r_error, p_definition.name + "." + String(event.name))) {
			if (!r_error.is_set()) {
				r_error = RNError::make(RNErrorCode::DUPLICATE_REGISTRATION, vformat("duplicate or colliding event member '%s'", event.subscription_name), "registerModule");
			}
			return false;
		}
		event_names.insert(event.name);
		members.insert(event.subscription_name);
	}
	definitions[p_definition.name] = p_definition;
	return true;
}

void RNNativeModuleRegistry::begin_generation(uint64_t p_generation) {
	if (generation == p_generation) {
		return;
	}
	for (const auto &entry : instances) {
		entry.value->shutdown();
	}
	instances.clear();
	jobs.clear();
	{
		std::lock_guard<std::mutex> lock(delivery_mutex);
		completions.clear();
		events.clear();
		event_bytes = 0;
		completion_bytes = 0;
		queued_completions.clear();
	}
	cancelled_requests.clear();
	module_cache.clear();
	pending_promises.clear();
	subscriptions.clear();
	next_request = 1;
	next_subscription = 1;
	generation = p_generation;
	accepting_work = true;
	objects.begin_generation(generation);
}

std::shared_ptr<RNNativeModule> RNNativeModuleRegistry::instance(const RNModuleDefinition &p_definition) {
	if (std::shared_ptr<RNNativeModule> *existing = instances.getptr(p_definition.name)) {
		return *existing;
	}
	std::unique_ptr<RNNativeModule> created = p_definition.factory();
	std::shared_ptr<RNNativeModule> shared(std::move(created));
	instances[p_definition.name] = shared;
	return shared;
}

bool RNNativeModuleRegistry::decode_arguments(facebook::jsi::Runtime &p_runtime, const String &p_module, const RNMethodSchema &p_schema, const facebook::jsi::Value *p_arguments, size_t p_count, Array &r_arguments, RNCallContext &r_context, RNError &r_error) {
	if (p_count > size_t(p_schema.arguments.size())) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "too many arguments", p_module + "." + String(p_schema.name), "args");
		return false;
	}
	for (int index = 0; index < p_schema.arguments.size(); ++index) {
		const RNArgumentSchema &argument = p_schema.arguments[index];
		if (size_t(index) >= p_count || p_arguments[index].isUndefined()) {
			if (argument.has_default) {
				r_arguments.push_back(argument.default_value);
				continue;
			}
			if (argument.optional) {
				r_arguments.push_back(Variant());
				continue;
			}
			r_error = RNError::make(RNErrorCode::VALIDATION, "required argument is absent", p_module + "." + String(p_schema.name), "args." + String(argument.name));
			return false;
		}
		RNValueSchema schema = argument.value;
		schema.nullable = schema.nullable || argument.nullable;
		RNDecodedValue decoded = RNValueCodec::from_js(p_runtime, p_arguments[index], schema, p_module + "." + String(p_schema.name), "args." + String(argument.name));
		if (!decoded.ok()) {
			r_error = decoded.error;
			return false;
		}
		r_arguments.push_back(decoded.value);
	}
	r_context.generation = generation;
	r_context.registry = shared_from_this();
	r_context.objects = &objects;
	if (p_schema.requires_session) {
		if (r_arguments.is_empty() || r_arguments[0].get_type() != Variant::STRING) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "session must be the first argument", p_module + "." + String(p_schema.name), "args.session");
			return false;
		}
		r_context.session_token = r_arguments[0];
		RNSessionRecord session;
		if (!objects.resolve_session(r_context.session_token, session, r_error)) {
			return false;
		}
		r_context.root_tag = session.root_tag;
		r_context.surface_epoch = session.surface_epoch;
	}
	return true;
}

facebook::jsi::Value RNNativeModuleRegistry::make_error(facebook::jsi::Runtime &p_runtime, const RNError &p_error) const {
	facebook::jsi::Function constructor = p_runtime.global().getPropertyAsFunction(p_runtime, "Error");
	facebook::jsi::Value message = facebook::jsi::String::createFromUtf8(p_runtime, to_utf8(p_error.message));
	facebook::jsi::Value created = constructor.callAsConstructor(p_runtime, message);
	facebook::jsi::Object object = created.getObject(p_runtime);
	for (const auto &entry : {
				 std::pair<const char *, String>("code", p_error.code),
				 std::pair<const char *, String>("operation", p_error.operation),
				 std::pair<const char *, String>("path", p_error.path) }) {
		object.setProperty(p_runtime, entry.first, facebook::jsi::String::createFromUtf8(p_runtime, to_utf8(entry.second)));
	}
	if (!p_error.module.is_empty()) {
		object.setProperty(p_runtime, "module", facebook::jsi::String::createFromUtf8(p_runtime, to_utf8(p_error.module)));
	}
	return facebook::jsi::Value(p_runtime, object);
}

facebook::jsi::Value RNNativeModuleRegistry::invoke_locked(facebook::jsi::Runtime &p_runtime, const String &p_module, const RNMethodSchema &p_schema, const facebook::jsi::Value *p_arguments, size_t p_count, bool p_include_request) {
	const RNModuleDefinition *module_definition = definition(p_module);
	if (!module_definition) {
		throw facebook::jsi::JSError(p_runtime, to_utf8("E_UNKNOWN_MODULE: " + p_module));
	}
	if (p_schema.mode == RNCallMode::SYNC) {
		Array arguments;
		RNCallContext context;
		RNError error;
		if (!decode_arguments(p_runtime, p_module, p_schema, p_arguments, p_count, arguments, context, error)) {
			throw facebook::jsi::JSError(p_runtime, make_error(p_runtime, error));
		}
		RNJSNativeCallScope native_call;
		RNModuleResult result = instance(*module_definition)->invoke_sync(p_schema.name, arguments, context);
		if (result.error.is_set()) {
			throw facebook::jsi::JSError(p_runtime, make_error(p_runtime, result.error));
		}
		facebook::jsi::Value converted;
		if (!RNValueCodec::to_js(p_runtime, result.value, p_schema.result, converted, error, nullptr, p_module + "." + String(p_schema.name), "result")) {
			throw facebook::jsi::JSError(p_runtime, make_error(p_runtime, error));
		}
		return converted;
	}

	if (pending_promises.size() >= 1024) {
		throw facebook::jsi::JSError(p_runtime, "Native request limit exceeded.");
	}
	const String request_token = vformat("request-%016x-%016x", generation, next_request++);
	auto capture = std::make_shared<PromiseCapture>();
	facebook::jsi::Function executor = facebook::jsi::Function::createFromHostFunction(
			p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "GodotPromiseExecutor"), 2,
			[capture](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
				if (count >= 2 && args[0].isObject() && args[1].isObject()) {
					capture->resolve = std::make_unique<facebook::jsi::Function>(facebook::jsi::Value(rt, args[0]).getObject(rt).getFunction(rt));
					capture->reject = std::make_unique<facebook::jsi::Function>(facebook::jsi::Value(rt, args[1]).getObject(rt).getFunction(rt));
				}
				return facebook::jsi::Value::undefined();
			});
	facebook::jsi::Function promise_constructor = p_runtime.global().getPropertyAsFunction(p_runtime, "Promise");
	facebook::jsi::Value promise = promise_constructor.callAsConstructor(p_runtime, executor);
	PendingPromise pending;
	pending.module_name = p_module;
	pending.method = p_schema.name;
	pending.result_schema = p_schema.result;
	pending.resolve = std::move(capture->resolve);
	pending.reject = std::move(capture->reject);
	Array arguments;
	RNCallContext context;
	RNError error;
	context.request_token = request_token;
	if (!decode_arguments(p_runtime, p_module, p_schema, p_arguments, p_count, arguments, context, error)) {
		if (pending.reject) {
			facebook::jsi::Value js_error = make_error(p_runtime, error);
			pending.reject->call(p_runtime, js_error);
		}
	} else if (!accepting_work) {
		RNError reset = RNError::make(RNErrorCode::RUNTIME_RESET, "native registry is resetting", p_module + "." + String(p_schema.name));
		if (pending.reject) {
			pending.reject->call(p_runtime, make_error(p_runtime, reset));
		}
	} else {
		pending.session_token = context.session_token;
		pending_promises.emplace(to_utf8(request_token), std::move(pending));
		NativeJob job;
		job.request_token = request_token;
		job.module_name = p_module;
		job.method = p_schema.name;
		job.arguments = arguments;
		job.context = context;
		job.context.request_token = request_token;
		jobs.push_back(job);
	}
	if (!p_include_request) {
		return promise;
	}
	facebook::jsi::Object result(p_runtime);
	result.setProperty(p_runtime, "requestId", facebook::jsi::String::createFromUtf8(p_runtime, to_utf8(request_token)));
	result.setProperty(p_runtime, "promise", facebook::jsi::Value(p_runtime, promise));
	return facebook::jsi::Value(p_runtime, result);
}

facebook::jsi::Value RNNativeModuleRegistry::subscribe_locked(facebook::jsi::Runtime &p_runtime, const String &p_module, const RNEventSchema &p_schema, const facebook::jsi::Value *p_arguments, size_t p_count) {
	const size_t callback_index = p_schema.requires_session ? 1 : 0;
	if (p_count != callback_index + 1 || !p_arguments[callback_index].isObject() || !p_arguments[callback_index].getObject(p_runtime).isFunction(p_runtime)) {
		throw facebook::jsi::JSError(p_runtime, "event subscription expects its exact session and callback arguments");
	}
	String session_token;
	if (p_schema.requires_session) {
		RNDecodedValue decoded = RNValueCodec::from_js(p_runtime, p_arguments[0], RNValueSchema::value(RNValueType::SESSION), p_module + "." + String(p_schema.subscription_name), "session");
		RNError error;
		RNSessionRecord session;
		if (!decoded.ok()) {
			throw facebook::jsi::JSError(p_runtime, make_error(p_runtime, decoded.error));
		}
		session_token = decoded.value;
		if (!objects.resolve_session(session_token, session, error)) {
			throw facebook::jsi::JSError(p_runtime, make_error(p_runtime, error));
		}
	}
	if (subscriptions.size() >= 4096) {
		throw facebook::jsi::JSError(p_runtime, "Native subscription limit exceeded.");
	}
	const std::string token = to_utf8(vformat("subscription-%016x-%016x", generation, next_subscription++));
	Subscription subscription;
	subscription.keeps_runtime_alive = p_schema.keeps_runtime_alive;
	subscription.origin = RNExecutionScope::current();
	subscription.module_name = p_module;
	subscription.event = p_schema.name;
	subscription.session_token = session_token;
	subscription.generation = generation;
	subscription.callback = std::make_unique<facebook::jsi::Function>(facebook::jsi::Value(p_runtime, p_arguments[callback_index]).getObject(p_runtime).getFunction(p_runtime));
	subscriptions.emplace(token, std::move(subscription));
	facebook::jsi::Object result(p_runtime);
	std::weak_ptr<RNNativeModuleRegistry> weak = shared_from_this();
	result.setProperty(
			p_runtime, "remove",
			facebook::jsi::Function::createFromHostFunction(
					p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "remove"), 0,
					[weak, token](facebook::jsi::Runtime &, const facebook::jsi::Value &, const facebook::jsi::Value *, size_t) {
						if (std::shared_ptr<RNNativeModuleRegistry> owner = weak.lock()) {
							owner->remove_subscription_locked(token);
						}
						return facebook::jsi::Value::undefined();
					}));
	return facebook::jsi::Value(p_runtime, result);
}

facebook::jsi::Value RNNativeModuleRegistry::module_for_locked(facebook::jsi::Runtime &p_runtime, const String &p_name) {
	if (!definition(p_name)) {
		return facebook::jsi::Value::null();
	}
	const std::string key = to_utf8(p_name);
	auto cached = module_cache.find(key);
	if (cached != module_cache.end()) {
		return facebook::jsi::Value(p_runtime, *cached->second);
	}
	facebook::jsi::Object object = facebook::jsi::Object::createFromHostObject(p_runtime, std::make_shared<RNModuleProxy>(shared_from_this(), p_name));
	module_cache.emplace(key, std::make_unique<facebook::jsi::Object>(facebook::jsi::Value(p_runtime, object).getObject(p_runtime)));
	return facebook::jsi::Value(p_runtime, object);
}

facebook::jsi::Value RNNativeModuleRegistry::get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(p_runtime);
	if (name == "get") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "get"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					if (count != 1 || !args[0].isString()) {
						return facebook::jsi::Value::null();
					}
					return module_for_locked(rt, from_utf8(args[0].getString(rt).utf8(rt)));
				});
	}
	if (name == "getSchema") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "getSchema"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					if (count != 1 || !args[0].isString()) {
						return facebook::jsi::Value::null();
					}
					const String module_name = from_utf8(args[0].getString(rt).utf8(rt));
					const RNModuleDefinition *module = definition(module_name);
					if (!module) {
						return facebook::jsi::Value::null();
					}
					facebook::jsi::Value result;
					RNError error;
					if (!RNValueCodec::to_js(rt, module_metadata(*module), RNValueSchema::value(RNValueType::DYNAMIC), result, error, nullptr, "NativeModuleRegistry.getSchema")) {
						throw facebook::jsi::JSError(rt, make_error(rt, error));
					}
					return result;
				});
	}
	if (name == "getStats") {
		return facebook::jsi::Function::createFromHostFunction(p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "getStats"), 0, [this](auto &rt, const auto &, const auto *, size_t) -> facebook::jsi::Value {
			Dictionary stats;
			stats["sessions"] = objects.session_count();
			stats["objects"] = objects.object_count();
			stats["subscriptions"] = int64_t(subscriptions.size());
			stats["requests"] = int64_t(pending_promises.size());
			{
				std::lock_guard<std::mutex> lock(delivery_mutex);
				stats["completionBytes"] = int64_t(completion_bytes);
			}
			facebook::jsi::Value value;
			RNError error;
			RNValueCodec::to_js(rt, stats, RNValueSchema::value(RNValueType::DYNAMIC), value, error, nullptr, "getStats");
			return value;
		});
	}
	if (name == "openSession") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "openSession"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					RNError error;
					if (count != 1) {
						throw facebook::jsi::JSError(rt, "openSession expects a root tag");
					}
					String token = objects.open_session(require_root_tag(rt, args[0]), error);
					if (error.is_set()) {
						throw facebook::jsi::JSError(rt, make_error(rt, error));
					}
					facebook::jsi::Value result;
					RNValueCodec::to_js(rt, token, RNValueSchema::value(RNValueType::SESSION), result, error, nullptr, "openSession");
					return result;
				});
	}
	if (name == "closeSession") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "closeSession"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					if (count != 1) {
						return facebook::jsi::Value(false);
					}
					RNDecodedValue decoded = RNValueCodec::from_js(rt, args[0], RNValueSchema::value(RNValueType::SESSION), "closeSession");
					if (!decoded.ok()) {
						return facebook::jsi::Value(false);
					}
					const String token = decoded.value;
					for (auto &entry : pending_promises) {
						if (entry.second.session_token == token) {
							cancel_request(from_utf8(entry.first), RNError::make(RNErrorCode::SESSION_CLOSED, "session was closed", "closeSession"));
						}
					}
					for (auto it = subscriptions.begin(); it != subscriptions.end();) {
						if (it->second.session_token == token) {
							it = subscriptions.erase(it);
						} else {
							++it;
						}
					}
					for (const auto &entry : instances) {
						entry.value->on_session_closed(token);
					}
					return facebook::jsi::Value(objects.close_session(token));
				});
	}
	if (name == "cancel") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "cancel"), 1,
				[this](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					if (count == 1 && args[0].isString()) {
						cancel_request(from_utf8(args[0].getString(rt).utf8(rt)), RNError::make(RNErrorCode::CANCELLED, "native call was cancelled", "cancel"));
					}
					return facebook::jsi::Value::undefined();
				});
	}
	return facebook::jsi::Value::undefined();
}

std::vector<facebook::jsi::PropNameID> RNNativeModuleRegistry::getPropertyNames(facebook::jsi::Runtime &p_runtime) {
	std::vector<facebook::jsi::PropNameID> names;
	for (const char *name : { "get", "getSchema", "openSession", "closeSession", "cancel" }) {
		names.push_back(facebook::jsi::PropNameID::forAscii(p_runtime, name));
	}
	return names;
}

facebook::jsi::Value RNNativeModuleRegistry::get_module_member(facebook::jsi::Runtime &p_runtime, const String &p_module, const std::string &p_member) {
	const RNModuleDefinition *module = definition(p_module);
	if (!module) {
		return facebook::jsi::Value::undefined();
	}
	if (p_member == "__godotStartAsync") {
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "__godotStartAsync"), 2,
				[this, p_module](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					if (count != 2 || !args[0].isString() || !args[1].isObject() || !args[1].getObject(rt).isArray(rt)) {
						throw facebook::jsi::JSError(rt, "__godotStartAsync expects a method name and argument array");
					}
					const RNModuleDefinition *definition_value = definition(p_module);
					const String method_name = from_utf8(args[0].getString(rt).utf8(rt));
					const RNMethodSchema *schema = definition_value ? method_schema(*definition_value, method_name) : nullptr;
					if (!schema || schema->mode != RNCallMode::ASYNC) {
						throw facebook::jsi::JSError(rt, "unknown async native module method");
					}
					facebook::jsi::Array array = args[1].getObject(rt).getArray(rt);
					std::vector<facebook::jsi::Value> values;
					for (size_t index = 0; index < array.size(rt); ++index) {
						values.push_back(array.getValueAtIndex(rt, index));
					}
					return invoke_locked(rt, p_module, *schema, values.data(), values.size(), true);
				});
	}
	if (const RNMethodSchema *schema = method_schema(*module, from_utf8(p_member))) {
		const RNMethodSchema schema_copy = *schema;
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forUtf8(p_runtime, p_member), schema_copy.arguments.size(),
				[this, p_module, schema_copy](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					return invoke_locked(rt, p_module, schema_copy, args, count, false);
				});
	}
	if (const RNEventSchema *schema = event_schema(*module, from_utf8(p_member))) {
		const RNEventSchema schema_copy = *schema;
		return facebook::jsi::Function::createFromHostFunction(
				p_runtime, facebook::jsi::PropNameID::forUtf8(p_runtime, p_member), schema_copy.requires_session ? 2 : 1,
				[this, p_module, schema_copy](facebook::jsi::Runtime &rt, const facebook::jsi::Value &, const facebook::jsi::Value *args, size_t count) {
					return subscribe_locked(rt, p_module, schema_copy, args, count);
				});
	}
	return facebook::jsi::Value::undefined();
}

facebook::jsi::Value RNModuleProxy::get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(p_runtime);
	if (name == "__godotNativeModuleBrand") {
		return facebook::jsi::Value(true);
	}
	if (std::shared_ptr<RNNativeModuleRegistry> owner = registry.lock()) {
		if (name == "__godotSchema") {
			facebook::jsi::Value result;
			RNError error;
			if (!RNValueCodec::to_js(p_runtime, owner->get_module_metadata(module_name), RNValueSchema::value(RNValueType::DYNAMIC), result, error, nullptr, "NativeModule.schema")) {
				throw facebook::jsi::JSError(p_runtime, to_utf8(error.describe()));
			}
			return result;
		}
		return owner->get_module_member(p_runtime, module_name, name);
	}
	return facebook::jsi::Value::undefined();
}

std::vector<facebook::jsi::PropNameID> RNModuleProxy::getPropertyNames(facebook::jsi::Runtime &p_runtime) {
	std::vector<facebook::jsi::PropNameID> names;
	names.push_back(facebook::jsi::PropNameID::forAscii(p_runtime, "__godotNativeModuleBrand"));
	names.push_back(facebook::jsi::PropNameID::forAscii(p_runtime, "__godotStartAsync"));
	names.push_back(facebook::jsi::PropNameID::forAscii(p_runtime, "__godotSchema"));
	if (std::shared_ptr<RNNativeModuleRegistry> owner = registry.lock()) {
		(void)owner;
	}
	return names;
}

void RNNativeModuleRegistry::process_jobs() {
	const auto shared = state.lock();
	const size_t count = std::min(jobs.size(), shared && shared->service_settings.limit("scheduler/max_tasks_per_frame") > 0 ? size_t(shared->service_settings.limit("scheduler/max_tasks_per_frame")) : size_t(256));
	for (size_t index = 0; index < count && !jobs.empty(); ++index) {
		NativeJob job = jobs.front();
		jobs.pop_front();
		if (job.context.generation != generation || pending_promises.find(to_utf8(job.request_token)) == pending_promises.end() || cancelled_requests.has(job.request_token)) {
			continue;
		}
		if (!job.context.session_token.is_empty()) {
			RNSessionRecord session;
			RNError error;
			if (!objects.resolve_session(job.context.session_token, session, error)) {
				queue_completion(job.request_token, generation, Variant(), error);
				continue;
			}
		}
		const RNModuleDefinition *module = definition(job.module_name);
		if (!module) {
			queue_completion(job.request_token, generation, Variant(), RNError::make(RNErrorCode::UNKNOWN_MODULE, "module disappeared", job.module_name + "." + String(job.method)));
			continue;
		}
		instance(*module)->start_async(job.method, job.arguments, job.context, RNCompletionToken(shared_from_this(), job.request_token, job.module_name, job.context.session_token, generation));
	}
}

namespace {
uint64_t delivery_size(const Variant &p_value, int p_depth = 0) {
	if (p_depth > 32) {
		return 16 * 1024 * 1024 + 1;
	}
	uint64_t bytes = 64;
	if (p_value.get_type() == Variant::PACKED_BYTE_ARRAY) {
		return bytes + PackedByteArray(p_value).size();
	}
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME) {
		return bytes + String(p_value).utf8().length();
	}
	if (p_value.get_type() == Variant::ARRAY) {
		for (const Variant &value : Array(p_value)) {
			bytes += delivery_size(value, p_depth + 1);
			if (bytes > 16 * 1024 * 1024) {
				break;
			}
		}
	} else if (p_value.get_type() == Variant::DICTIONARY) {
		const Dictionary values = p_value;
		for (const Variant &key : values.keys()) {
			bytes += delivery_size(key, p_depth + 1) + delivery_size(values[key], p_depth + 1);
			if (bytes > 16 * 1024 * 1024) {
				break;
			}
		}
	}
	return bytes;
}
} //namespace
void RNNativeModuleRegistry::queue_completion(const String &p_request_token, uint64_t p_generation, const Variant &p_value, const RNError &p_error) {
	std::lock_guard<std::mutex> lock(delivery_mutex);
	if (!accepting_work || p_generation != generation || queued_completions.has(p_request_token) || queued_completions.size() >= 1024) {
		return;
	}
	NativeCompletion completion;
	completion.request_token = p_request_token;
	completion.generation = p_generation;
	completion.error = p_error;
	const uint64_t size = delivery_size(p_error.is_set() ? Variant(p_error.to_dictionary()) : p_value);
	if (size > 16 * 1024 * 1024 - completion_bytes) {
		completion.error = RNError::make(RNErrorCode::LIMIT, "native completion aggregate byte limit exceeded", "native.complete");
	} else {
		if (!p_error.is_set()) {
			completion.value = p_value.duplicate(true);
		}
		completion.bytes = size;
		completion_bytes += size;
	}
	queued_completions.insert(p_request_token);
	completion.order = next_delivery++;
	completions.push_back(std::move(completion));
}

bool RNNativeModuleRegistry::can_queue_event(uint64_t p_bytes) const {
	std::lock_guard<std::mutex> lock(delivery_mutex);
	return events.size() < 1024 && p_bytes <= 16 * 1024 * 1024 - event_bytes;
}
bool RNNativeModuleRegistry::queue_event(const String &p_module, const StringName &p_event, const String &p_session, uint64_t p_generation, const Variant &p_payload) {
	NativeEvent event;
	event.module_name = p_module;
	event.event = p_event;
	event.session_token = p_session;
	event.generation = p_generation;
	event.payload = p_payload.duplicate(true);
	std::lock_guard<std::mutex> lock(delivery_mutex);
	const uint64_t size = delivery_size(p_payload);
	if (events.size() >= 1024 || size > 16 * 1024 * 1024 - event_bytes) {
		return false;
	}
	event_bytes += size;
	event.order = next_delivery++;
	events.push_back(event);
	return true;
}

void RNNativeModuleRegistry::cancel_request(const String &p_request_token, const RNError &p_error) {
	auto pending = pending_promises.find(to_utf8(p_request_token));
	if (pending == pending_promises.end() || pending->second.settled || cancelled_requests.has(p_request_token)) {
		return;
	}
	cancelled_requests.insert(p_request_token);
	pending->second.cancellation_error = p_error;
	NativeCompletion cancellation;
	cancellation.request_token = p_request_token;
	cancellation.generation = generation;
	cancellation.error = p_error;
	{
		std::lock_guard<std::mutex> lock(delivery_mutex);
		completions.erase(
				std::remove_if(completions.begin(), completions.end(), [this, &p_request_token](const NativeCompletion &p_completion) {
					if (p_completion.request_token != p_request_token) {
						return false;
					}
					completion_bytes -= p_completion.bytes;
					return true;
				}),
				completions.end());
		queued_completions.insert(p_request_token);
		cancellation.order = next_delivery++;
		completions.push_back(cancellation);
	}
	if (std::shared_ptr<RNNativeModule> *module = instances.getptr(pending->second.module_name)) {
		(*module)->cancel(p_request_token);
	}
}

void RNNativeModuleRegistry::reject_promise_locked(facebook::jsi::Runtime &p_runtime, PendingPromise &p_pending, const RNError &p_error) {
	if (p_pending.reject) {
		p_pending.reject->call(p_runtime, make_error(p_runtime, p_error));
	}
	p_pending.settled = true;
}

size_t RNNativeModuleRegistry::deliver_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation, const std::function<void()> &p_checkpoint) {
	const auto shared = state.lock();
	const size_t maximum = shared && shared->service_settings.limit("scheduler/max_tasks_per_frame") > 0 ? size_t(shared->service_settings.limit("scheduler/max_tasks_per_frame")) : size_t(256);
	size_t delivered = 0;
	std::deque<std::variant<NativeCompletion, NativeEvent>> batch;
	{
		std::lock_guard<std::mutex> lock(delivery_mutex);
		while (batch.size() < maximum && (!completions.empty() || !events.empty())) {
			if (events.empty() || (!completions.empty() && completions.front().order < events.front().order)) {
				batch.emplace_back(std::move(completions.front()));
				completions.pop_front();
			} else {
				batch.emplace_back(std::move(events.front()));
				events.pop_front();
			}
		}
	}
	while (!batch.empty() && delivered < maximum) {
		auto item = std::move(batch.front());
		batch.pop_front();
		if (auto native_completion = std::get_if<NativeCompletion>(&item)) {
			NativeCompletion &completion = *native_completion;
			struct CompletionAccounting {
				RNNativeModuleRegistry &registry;
				const NativeCompletion &completion;
				~CompletionAccounting() {
					std::lock_guard<std::mutex> lock(registry.delivery_mutex);
					if (registry.generation == completion.generation && registry.accepting_work) {
						registry.completion_bytes -= completion.bytes;
						registry.queued_completions.erase(completion.request_token);
					}
				}
			} accounting{ *this, completion };
			if (completion.generation != generation || completion.generation != p_generation) {
				continue;
			}
			auto pending = pending_promises.find(to_utf8(completion.request_token));
			if (pending == pending_promises.end() || pending->second.settled) {
				continue;
			}
			PendingPromise &promise = pending->second;
			if (promise.cancellation_error.is_set()) {
				completion.error = promise.cancellation_error;
			}
			if (!promise.session_token.is_empty()) {
				RNSessionRecord session;
				RNError session_error;
				if (!objects.resolve_session(promise.session_token, session, session_error)) {
					completion.error = session_error;
				}
			}
			bool success = false;
			if (completion.error.is_set()) {
				reject_promise_locked(p_runtime, promise, completion.error);
			} else {
				facebook::jsi::Value result;
				RNError error;
				if (!RNValueCodec::to_js(p_runtime, completion.value, promise.result_schema, result, error, nullptr, promise.module_name + "." + String(promise.method), "result")) {
					reject_promise_locked(p_runtime, promise, error);
				} else if (promise.resolve) {
					promise.resolve->call(p_runtime, result);
					success = true;
					promise.settled = true;
				}
			}
			if (auto module = instances.getptr(promise.module_name)) {
				(*module)->on_result_delivered(completion.request_token, success);
			}
			pending_promises.erase(pending);
			cancelled_requests.erase(completion.request_token);
			++delivered;
			if (p_checkpoint) {
				p_checkpoint();
			}
			continue;
		}
		NativeEvent &event = std::get<NativeEvent>(item);
		struct EventAccounting {
			RNNativeModuleRegistry &registry;
			uint64_t bytes;
			bool retained = false;
			~EventAccounting() {
				if (!retained) {
					std::lock_guard<std::mutex> lock(registry.delivery_mutex);
					registry.event_bytes -= bytes;
				}
			}
		} accounting{ *this, delivery_size(event.payload) };
		if (event.generation != generation || event.generation != p_generation) {
			continue;
		}
		const RNModuleDefinition *module = definition(event.module_name);
		const RNEventSchema *schema = module ? [&]() -> const RNEventSchema * {
			for (const RNEventSchema &candidate : module->events) {
				if (candidate.name == event.event) {
					return &candidate;
				}
			}
			return nullptr;
		}()
				: nullptr;
		if (!schema) {
			continue;
		}
		facebook::jsi::Value payload;
		RNError error;
		if (!RNValueCodec::to_js(p_runtime, event.payload, schema->payload, payload, error, nullptr, event.module_name + "." + String(event.event), "event")) {
			WARN_PRINT(error.describe());
			continue;
		}
		if (!event.listeners_captured) {
			event.listeners_captured = true;
			for (const auto &entry : subscriptions) {
				const Subscription &subscription = entry.second;
				if (subscription.generation != generation || subscription.module_name != event.module_name || subscription.event != event.event || subscription.session_token != event.session_token || !subscription.callback) {
					continue;
				}
				event.listeners.push_back(entry.first);
			}
		}
		while (event.next_listener < event.listeners.size() && delivered < maximum) {
			const std::string token = event.listeners[event.next_listener++];
			auto found = subscriptions.find(token);
			if (found == subscriptions.end()) {
				continue;
			}
			facebook::jsi::Function callback = facebook::jsi::Value(p_runtime, *found->second.callback).getObject(p_runtime).getFunction(p_runtime);
			try {
				RNExecutionOrigin origin = found->second.origin;
				if (!found->second.session_token.is_empty()) {
					RNSessionRecord session;
					RNError ignored;
					if (objects.resolve_session(found->second.session_token, session, ignored)) {
						origin = { session.generation, session.root_tag, session.surface_epoch };
					}
				}
				RNExecutionScope scope(origin);
				callback.call(p_runtime, payload);
			} catch (const facebook::jsi::JSIException &exception) {
				WARN_PRINT(from_utf8(exception.what()));
			}
			++delivered;
			if (p_checkpoint) {
				p_checkpoint();
			}
		}
		if (event.next_listener < event.listeners.size()) {
			accounting.retained = true;
			batch.push_front(std::move(item));
			break;
		}
	}
	if (!batch.empty()) {
		std::lock_guard<std::mutex> lock(delivery_mutex);
		while (!batch.empty()) {
			if (auto completion = std::get_if<NativeCompletion>(&batch.back())) {
				completions.push_front(std::move(*completion));
			} else {
				events.push_front(std::move(std::get<NativeEvent>(batch.back())));
			}
			batch.pop_back();
		}
	}
	return delivered;
}

void RNNativeModuleRegistry::remove_subscription_locked(const std::string &p_token) {
	subscriptions.erase(p_token);
}

void RNNativeModuleRegistry::close_surface(int p_root_tag, uint64_t p_epoch) {
	for (const auto &entry : instances) {
		entry.value->on_surface_closed(p_root_tag, p_epoch);
	}
	for (auto &entry : pending_promises) {
		if (objects.session_matches(entry.second.session_token, p_root_tag, p_epoch)) {
			cancel_request(from_utf8(entry.first), RNError::make(RNErrorCode::SESSION_CLOSED, "surface session was closed", "surface.close"));
		}
	}
	for (auto it = subscriptions.begin(); it != subscriptions.end();) {
		if (objects.session_matches(it->second.session_token, p_root_tag, p_epoch)) {
			it = subscriptions.erase(it);
		} else {
			++it;
		}
	}
	objects.close_surface(p_root_tag, p_epoch);
}

void RNNativeModuleRegistry::before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) {
	(void)p_runtime;
	(void)p_generation;
	accepting_work = false;
	if (auto shared = state.lock()) {
		if (shared->http && ObjectDB::get_instance(shared->http_id)) {
			shared->http->shutdown();
		}
		if (shared->blobs) {
			shared->blobs->shutdown();
		}
	}
	jobs.clear();
	{
		std::lock_guard<std::mutex> lock(delivery_mutex);
		completions.clear();
		events.clear();
		event_bytes = 0;
		completion_bytes = 0;
		queued_completions.clear();
	}
	cancelled_requests.clear();
	pending_promises.clear();
	subscriptions.clear();
	module_cache.clear();
	for (const auto &entry : instances) {
		entry.value->shutdown();
	}
	instances.clear();
	objects.clear_generation();
}

bool RNNativeModuleRegistry::has_pending_work() const {
	for (const auto &entry : instances) {
		if (entry.value->has_pending_work()) {
			return true;
		}
	}
	for (const auto &entry : subscriptions) {
		if (entry.second.keeps_runtime_alive) {
			return true;
		}
	}
	std::lock_guard<std::mutex> lock(delivery_mutex);
	return !jobs.empty() || !completions.empty() || !events.empty() || !pending_promises.empty();
}

void RNNativeModuleRegistry::process_frame(double p_now_ms) {
	for (const auto &entry : instances) {
		entry.value->process_frame(p_now_ms);
	}
}
void RNNativeModuleRegistry::scene_binding_changed(ObjectID p_root) {
	for (const auto &entry : instances) {
		entry.value->on_scene_binding_changed(p_root);
	}
}
