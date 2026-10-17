#include "rn_godot_scene_module.h"

#include "../host/rn_main_thread_queue.h"
#include "../interop/rn_value_codec.h"
#include "../root_view/react_native_root_view.h"
#include "rn_js_promise.h"

#include "core/os/thread.h"

namespace {

class SceneSignalCallable : public CallableCustom {
	ObjectID target;
	std::function<void(const Variant **, int)> callback;
	static bool equal(const CallableCustom *p_a, const CallableCustom *p_b) { return p_a == p_b; }
	static bool less(const CallableCustom *p_a, const CallableCustom *p_b) { return std::less<const CallableCustom *>()(p_a, p_b); }

public:
	SceneSignalCallable(ObjectID p_target, std::function<void(const Variant **, int)> p_callback) :
			target(p_target), callback(std::move(p_callback)) {}
	uint32_t hash() const override { return uint32_t(reinterpret_cast<uintptr_t>(this)); }
	String get_as_text() const override { return "RNSceneSignal"; }
	CompareEqualFunc get_compare_equal_func() const override { return equal; }
	CompareLessFunc get_compare_less_func() const override { return less; }
	ObjectID get_object() const override { return target; }
	void call(const Variant **p_arguments, int p_count, Variant &, Callable::CallError &r_error) const override {
		r_error.error = Callable::CallError::CALL_OK;
		ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "Scene capability signals must be emitted on the main thread.");
		callback(p_arguments, p_count);
	}
};

bool live_target(ObjectID p_target) {
	Object *target = ObjectDB::get_instance(p_target);
	Node *node = Object::cast_to<Node>(target);
	return target && (!node || node->is_inside_tree());
}

enum class SceneValueMode {
	TO_SCRIPT,
	TO_TOKEN,
	TO_WRAPPER,
};

struct SceneValueContext {
	RNObjectRegistry &objects;
	int root_tag = 0;
	uint32_t visited = 0;
};

bool convert_scene_value(const Variant &p_value, const RNValueSchema &p_schema, SceneValueContext &r_context, SceneValueMode p_mode, Variant &r_value, RNError &r_error, uint32_t p_depth = 0) {
	if (++r_context.visited > 65536 || p_depth > 32) {
		r_error = RNError::make(RNErrorCode::LIMIT, "scene value exceeds structural limits", "GodotScene", "value");
		return false;
	}
	r_value = p_value;
	if (p_value.get_type() == Variant::NIL) {
		return true;
	}
	if (p_schema.type == RNValueType::OBJECT && p_mode == SceneValueMode::TO_TOKEN) {
		if (p_value.get_type() != Variant::OBJECT) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "scene result requires an Object", "GodotScene", "value");
			return false;
		}
		bool previously_freed = false;
		Object *object = p_value.get_validated_object_with_check(previously_freed);
		if (!object) {
			if (previously_freed) {
				r_error = RNError::make(RNErrorCode::OBJECT_GONE, "scene result Object was destroyed", "GodotScene", "value");
				return false;
			}
			r_value = Variant();
			return true;
		}
		r_value = r_context.objects.register_object(r_context.root_tag, object->get_instance_id(), p_schema.capability, r_error);
		return !r_error.is_set();
	}
	if (p_schema.type == RNValueType::OBJECT && p_mode == SceneValueMode::TO_SCRIPT) {
		Object *object = r_context.objects.resolve_object(p_value, r_context.root_tag, p_schema.capability, r_error);
		if (!object) {
			return false;
		}
		r_value = object;
		return true;
	}
	if (p_mode == SceneValueMode::TO_WRAPPER && p_schema.type == RNValueType::OBJECT) {
		Dictionary wrapper;
		wrapper["$godot"] = "Object";
		wrapper["handle"] = p_value;
		r_value = wrapper;
	} else if (p_mode == SceneValueMode::TO_WRAPPER && p_schema.type == RNValueType::INT64) {
		Dictionary wrapper;
		wrapper["$godot"] = "int64";
		wrapper["value"] = String::num_int64(int64_t(p_value));
		r_value = wrapper;
	} else if (p_schema.type == RNValueType::ARRAY && p_value.get_type() == Variant::ARRAY) {
		const Array source = p_value;
		if (source.size() > 4096) {
			r_error = RNError::make(RNErrorCode::LIMIT, "scene array exceeds entry limit", "GodotScene", "value");
			return false;
		}
		Array converted;
		for (const Variant &element : source) {
			Variant value;
			if (!convert_scene_value(element, *p_schema.element, r_context, p_mode, value, r_error, p_depth + 1)) {
				return false;
			}
			converted.push_back(value);
		}
		r_value = converted;
	} else if (p_schema.type == RNValueType::RECORD && p_value.get_type() == Variant::DICTIONARY) {
		Dictionary converted = Dictionary(p_value).duplicate();
		if (converted.size() > 4096) {
			r_error = RNError::make(RNErrorCode::LIMIT, "scene record exceeds entry limit", "GodotScene", "value");
			return false;
		}
		for (const RNRecordFieldSchema &field : p_schema.fields) {
			if (converted.has(field.name)) {
				Variant value;
				if (!convert_scene_value(converted[field.name], *field.value, r_context, p_mode, value, r_error, p_depth + 1)) {
					return false;
				}
				converted[field.name] = value;
			}
		}
		r_value = converted;
	}
	return true;
}

// Turns a script value into what JavaScript receives: Objects become handles,
// then the value is checked against its schema and wrapped.
bool export_scene_value(const Variant &p_value, const RNValueSchema &p_schema, RNObjectRegistry &p_objects, int p_root_tag, Variant &r_value, RNError &r_error) {
	SceneValueContext context{ p_objects, p_root_tag };
	Variant tokens;
	if (!convert_scene_value(p_value, p_schema, context, SceneValueMode::TO_TOKEN, tokens, r_error) || !rn_validate_native_value(tokens, p_schema, r_error, "value")) {
		return false;
	}
	context.visited = 0;
	return convert_scene_value(rn_apply_native_defaults(tokens, p_schema), p_schema, context, SceneValueMode::TO_WRAPPER, r_value, r_error);
}

facebook::jsi::Value to_js(facebook::jsi::Runtime &p_runtime, const Variant &p_value) {
	facebook::jsi::Value result;
	RNError error;
	if (!RNValueCodec::to_js(p_runtime, p_value, RNValueSchema::value(RNValueType::DYNAMIC), result, error)) {
		rn_throw_js_error(p_runtime, error.code.utf8().get_data(), error.message.utf8().get_data());
	}
	return result;
}

} // namespace

void RNSceneBindings::add_emitter(std::function<bool(const Variant &)> p_emit) {
	std::lock_guard lock(emit_mutex);
	emitters.push_back(std::move(p_emit));
}

void RNSceneBindings::_disconnect(Binding &p_binding) {
	for (const Connection &connection : p_binding.connections) {
		Object *target = ObjectDB::get_instance(connection.target);
		if (target && target->is_connected(connection.signal, connection.callback)) {
			target->disconnect(connection.signal, connection.callback);
		}
	}
	p_binding.connections.clear();
	if (!p_binding.handle.is_empty()) {
		objects.unregister_handle(p_binding.handle);
	}
	p_binding.handle = String();
	p_binding.attachment.reset();
}

Dictionary RNSceneBindings::_envelope(const Binding &p_binding) const {
	Dictionary value;
	value["binding"] = p_binding.handle.is_empty() ? Variant() : Variant(p_binding.handle);
	value["ready"] = !p_binding.handle.is_empty();
	value["sequence"] = int64_t(sequence);
	if (p_binding.attachment) {
		value["capability"] = p_binding.attachment->capability;
		value["schemaVersion"] = p_binding.attachment->schema_version;
	}
	return value;
}

void RNSceneBindings::_publish(int p_root_tag, const Binding &p_binding, const StringName &p_event, const Variant &p_payload) {
	++sequence;
	Dictionary change = _envelope(p_binding);
	change["rootTag"] = p_root_tag;
	change["event"] = String(p_event);
	if (p_payload.get_type() != Variant::NIL) {
		change["payload"] = p_payload;
	}
	std::lock_guard lock(emit_mutex);
	std::erase_if(emitters, [&change](const std::function<bool(const Variant &)> &p_emit) { return !p_emit(change); });
}

bool RNSceneBindings::_refresh(int p_root_tag, Binding &p_binding, RNError &r_error) {
	ReactNativeRootView *root = find_root(p_root_tag);
	auto attachment = root ? root->get_scene_attachment() : nullptr;
	if (attachment && !live_target(attachment->target)) {
		attachment.reset();
	}
	if (attachment == p_binding.attachment) {
		return true;
	}
	_disconnect(p_binding);
	if (!attachment) {
		_publish(p_root_tag, p_binding, "ready", Variant());
		return true;
	}
	p_binding.handle = objects.register_object(p_root_tag, attachment->target, attachment->capability, r_error);
	if (r_error.is_set()) {
		return false;
	}
	p_binding.attachment = attachment;
	Object *target = ObjectDB::get_instance(attachment->target);
	for (const KeyValue<StringName, RNSceneSignal> &entry : attachment->signals) {
		const uint64_t identity = attachment->identity;
		const RNSceneSignal signal = entry.value;
		Callable callback(memnew(SceneSignalCallable(attachment->target, [this, p_root_tag, identity, signal](const Variant **p_arguments, int p_count) {
			auto found = bindings.find(p_root_tag);
			if (found == bindings.end() || !found->second.attachment || found->second.attachment->identity != identity || p_count != signal.arguments.size()) {
				return;
			}
			Dictionary payload;
			for (int i = 0; i < p_count; ++i) {
				payload[signal.arguments[i]] = *p_arguments[i];
			}
			RNError error;
			Variant converted;
			if (!export_scene_value(payload, signal.payload, objects, p_root_tag, converted, error)) {
				ERR_PRINT(error.describe());
				return;
			}
			_publish(p_root_tag, found->second, signal.event, converted);
		})));
		if (target->connect(entry.key, callback) != OK) {
			_disconnect(p_binding);
			r_error = RNError::make(RNErrorCode::NATIVE, "signal connection failed", "GodotScene.getBinding");
			return false;
		}
		p_binding.connections.push_back({ attachment->target, entry.key, callback });
	}
	_publish(p_root_tag, p_binding, "ready", Variant());
	return true;
}

RNSceneBindings::Binding *RNSceneBindings::_binding(int p_root_tag, RNError &r_error) {
	if (!find_root(p_root_tag)) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, vformat("root %d is not running", p_root_tag), "GodotScene", "rootTag");
		return nullptr;
	}
	Binding &binding = bindings[p_root_tag];
	return _refresh(p_root_tag, binding, r_error) ? &binding : nullptr;
}

bool RNSceneBindings::get_binding(int p_root_tag, Dictionary &r_binding, RNError &r_error) {
	Binding *binding = _binding(p_root_tag, r_error);
	if (!binding) {
		return false;
	}
	r_binding = _envelope(*binding);
	return true;
}

bool RNSceneBindings::invoke(int p_root_tag, const String &p_handle, const StringName &p_command, const Array &p_arguments, bool p_async, Variant &r_result, RNError &r_error) {
	Binding *binding = _binding(p_root_tag, r_error);
	if (!binding) {
		return false;
	}
	if (!binding->attachment || binding->handle != p_handle) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "binding is missing or has changed", "GodotScene");
		return false;
	}
	Object *target = objects.resolve_object(p_handle, p_root_tag, binding->attachment->capability, r_error);
	if (!target) {
		return false;
	}
	const std::shared_ptr<const RNSceneAttachment> attachment = binding->attachment;
	StringName method = attachment->snapshot_method;
	RNValueSchema result_schema = attachment->snapshot_schema;
	Array arguments;
	if (p_command != StringName()) {
		const RNSceneCommand *command = attachment->commands.getptr(p_command);
		if (!command || (command->schema.mode == RNCallMode::ASYNC) != p_async) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "command is not declared in this mode", "GodotScene.call");
			return false;
		}
		method = command->method;
		result_schema = command->schema.result;
		if (p_arguments.size() > command->schema.arguments.size()) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "too many command arguments", "GodotScene.call");
			return false;
		}
		SceneValueContext context{ objects, p_root_tag };
		for (int index = 0; index < command->schema.arguments.size(); ++index) {
			const RNArgumentSchema &argument = command->schema.arguments[index];
			const bool supplied = index < p_arguments.size();
			if (!supplied && argument.optional && !argument.has_default) {
				break;
			}
			if (!supplied && !argument.has_default) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "missing command argument", "GodotScene.call");
				return false;
			}
			RNValueSchema schema = argument.value;
			schema.nullable = schema.nullable || argument.nullable;
			const Variant value = supplied ? p_arguments[index] : argument.default_value;
			Variant converted;
			if (!rn_validate_native_value(value, schema, r_error, "command.arguments") || !convert_scene_value(rn_apply_native_defaults(value, schema), schema, context, SceneValueMode::TO_SCRIPT, converted, r_error)) {
				return false;
			}
			arguments.push_back(converted);
		}
	}
	Vector<const Variant *> pointers;
	for (int i = 0; i < arguments.size(); ++i) {
		pointers.push_back(&arguments[i]);
	}
	Callable::CallError call_error;
	const Variant value = target->callp(method, pointers.ptrw(), pointers.size(), call_error);
	if (call_error.error != Callable::CallError::CALL_OK) {
		r_error = RNError::make(RNErrorCode::NATIVE, "script invocation failed", "GodotScene.call");
		return false;
	}
	auto current = bindings.find(p_root_tag);
	if (current == bindings.end() || current->second.attachment != attachment || current->second.handle != p_handle || !live_target(attachment->target)) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "binding changed during script invocation", "GodotScene.call");
		return false;
	}
	Variant converted;
	if (!export_scene_value(value, result_schema, objects, p_root_tag, converted, r_error)) {
		return false;
	}
	if (p_command == StringName()) {
		Dictionary snapshot = _envelope(current->second);
		snapshot["payload"] = converted;
		r_result = snapshot;
	} else {
		r_result = converted;
	}
	return true;
}

void RNSceneBindings::binding_changed(int p_root_tag) {
	auto found = bindings.find(p_root_tag);
	if (found != bindings.end()) {
		RNError error;
		if (!_refresh(p_root_tag, found->second, error)) {
			ERR_PRINT(error.describe());
		}
	}
}

void RNSceneBindings::close_surface(int p_root_tag) {
	auto found = bindings.find(p_root_tag);
	if (found != bindings.end()) {
		_disconnect(found->second);
		bindings.erase(found);
	}
	objects.close_surface(p_root_tag);
}

void RNSceneBindings::process_frame() {
	for (auto &[root_tag, binding] : bindings) {
		if (binding.attachment && !live_target(binding.attachment->target)) {
			RNError error;
			_refresh(root_tag, binding, error);
		}
	}
}

void RNSceneBindings::shutdown() {
	for (auto &[root_tag, binding] : bindings) {
		_disconnect(binding);
	}
	bindings.clear();
}

namespace facebook::react {

RNGodotSceneModule::RNGodotSceneModule(std::shared_ptr<CallInvoker> jsInvoker, RNSceneModuleContext p_context) :
		NativeGodotSceneCxxSpec(std::move(jsInvoker)), context(std::move(p_context)) {
}

jsi::Object RNGodotSceneModule::getBinding(jsi::Runtime &rt, double rootTag) {
	Dictionary binding;
	RNError error;
	if (!context.main_thread->run_sync([&] { context.scenes->get_binding(int(rootTag), binding, error); })) {
		rn_throw_js_error(rt, RNErrorCode::CANCELLED, "React Native is shutting down");
	}
	if (error.is_set()) {
		rn_throw_js_error(rt, error.code.utf8().get_data(), error.message.utf8().get_data());
	}
	return to_js(rt, binding).asObject(rt);
}

jsi::Object RNGodotSceneModule::read(jsi::Runtime &rt, double rootTag, const std::string &binding) {
	return _invoke(rt, int(rootTag), binding, std::string(), jsi::Value::undefined(), false).asObject(rt);
}

jsi::Value RNGodotSceneModule::call(jsi::Runtime &rt, double rootTag, const std::string &binding, const std::string &command, jsi::Array args) {
	return _invoke(rt, int(rootTag), binding, command, jsi::Value(rt, args), false);
}

jsi::Value RNGodotSceneModule::_invoke(jsi::Runtime &rt, int p_root_tag, const std::string &p_handle, const std::string &p_command, const jsi::Value &p_arguments, bool p_async) {
	Array arguments;
	if (!p_arguments.isUndefined()) {
		RNDecodedValue decoded = RNValueCodec::from_js(rt, p_arguments, RNValueSchema::array(RNValueSchema::value(RNValueType::DYNAMIC)), "GodotScene.call", "args");
		if (!decoded.ok()) {
			rn_throw_js_error(rt, decoded.error.code.utf8().get_data(), decoded.error.message.utf8().get_data());
		}
		arguments = decoded.value;
	}
	Variant result;
	RNError error;
	const String handle = String::utf8(p_handle.c_str());
	const StringName command = p_command.empty() ? StringName() : StringName(String::utf8(p_command.c_str()));
	if (!context.main_thread->run_sync([&] { context.scenes->invoke(p_root_tag, handle, command, arguments, p_async, result, error); })) {
		rn_throw_js_error(rt, RNErrorCode::CANCELLED, "React Native is shutting down");
	}
	if (error.is_set()) {
		rn_throw_js_error(rt, error.code.utf8().get_data(), error.message.utf8().get_data());
	}
	return to_js(rt, result);
}

jsi::Value RNGodotSceneModule::callAsync(jsi::Runtime &rt, double rootTag, const std::string &binding, const std::string &command, jsi::Array args) {
	RNDecodedValue decoded = RNValueCodec::from_js(rt, jsi::Value(rt, args), RNValueSchema::array(RNValueSchema::value(RNValueType::DYNAMIC)), "GodotScene.callAsync", "args");
	if (!decoded.ok()) {
		rn_throw_js_error(rt, decoded.error.code.utf8().get_data(), decoded.error.message.utf8().get_data());
	}
	const int root_tag = int(rootTag);
	const String handle = String::utf8(binding.c_str());
	const StringName name(String::utf8(command.c_str()));
	const Array arguments = decoded.value;
	auto scenes = context.scenes;
	auto main_thread = context.main_thread;
	return RNJSPromise::create(rt, jsInvoker_, [=](std::shared_ptr<RNJSPromise> p_promise) {
		main_thread->post([=] {
			Variant result;
			RNError error;
			if (!scenes->invoke(root_tag, handle, name, arguments, true, result, error)) {
				p_promise->reject(error.code.utf8().get_data(), error.message.utf8().get_data());
				return;
			}
			p_promise->resolve([result](jsi::Runtime &p_runtime) { return to_js(p_runtime, result); });
		});
	});
}

void RNGodotSceneModule::emit_changed(const Variant &p_change) {
	static_cast<AsyncEventEmitter<jsi::Value> &>(*eventEmitterMap_["onChanged"]).emit([p_change](jsi::Runtime &p_runtime) -> jsi::Value {
		jsi::Value result;
		RNError error;
		return RNValueCodec::to_js(p_runtime, p_change, RNValueSchema::value(RNValueType::DYNAMIC), result, error) ? std::move(result) : jsi::Value::null();
	});
}

} // namespace facebook::react
