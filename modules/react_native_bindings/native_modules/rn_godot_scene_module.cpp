#include "rn_godot_scene_module.h"

#include "../interop/rn_scene_binding.h"
#include "../root_view/react_native_root_view.h"

#include "core/error/error_macros.h"
#include "core/object/object.h"
#include "core/os/thread.h"
#include "scene/main/node.h"

#include <functional>

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
class RNGodotSceneModule : public RNNativeModule {
	struct Connection {
		ObjectID target;
		StringName signal;
		Callable callback;
	};
	struct Binding {
		RNCallContext context;
		ObjectID root;
		std::shared_ptr<const RNSceneAttachment> attachment;
		String handle;
		Vector<Connection> connections;
	};
	HashMap<String, Binding> bindings;
	uint64_t sequence = 0;

	void disconnect(Binding &p_binding) {
		for (const Connection &connection : p_binding.connections) {
			Object *target = ObjectDB::get_instance(connection.target);
			if (target && target->is_connected(connection.signal, connection.callback)) {
				target->disconnect(connection.signal, connection.callback);
			}
		}
		p_binding.connections.clear();
		if (p_binding.context.objects && !p_binding.handle.is_empty()) {
			p_binding.context.objects->unregister_handle(p_binding.handle);
		}
		p_binding.handle = String();
		p_binding.attachment.reset();
	}
	Dictionary envelope(const Binding &p_binding, const StringName &p_event, const Variant &p_payload) const {
		Dictionary value;
		value["binding"] = p_binding.handle.is_empty() ? Variant() : Variant(p_binding.handle);
		value["ready"] = !p_binding.handle.is_empty();
		value["sequence"] = int64_t(sequence);
		value["event"] = String(p_event);
		value["payload"] = p_payload;
		if (p_binding.attachment) {
			value["capability"] = p_binding.attachment->capability;
			value["schemaVersion"] = p_binding.attachment->schema_version;
		}
		return value;
	}
	void publish(const Binding &p_binding, const StringName &p_event, const Variant &p_payload) {
		ERR_FAIL_COND_MSG(sequence >= 9007199254740991ULL, "Scene sequence exhausted.");
		++sequence;
		if (auto registry = p_binding.context.registry.lock()) {
			registry->queue_event("GodotScene", "changed", p_binding.context.session_token, p_binding.context.generation, envelope(p_binding, p_event, p_payload));
		}
	}
	bool refresh(Binding &p_binding, RNError &r_error) {
		ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(ObjectDB::get_instance(p_binding.root));
		auto attachment = root ? root->get_scene_attachment() : nullptr;
		if (attachment && !live_target(attachment->target)) {
			attachment.reset();
		}
		if (attachment == p_binding.attachment) {
			return true;
		}
		disconnect(p_binding);
		if (!attachment) {
			publish(p_binding, "ready", Variant());
			return true;
		}
		p_binding.handle = p_binding.context.objects->register_object(p_binding.context.session_token, attachment->target, attachment->capability, r_error);
		if (r_error.is_set()) {
			return false;
		}
		p_binding.attachment = attachment;
		Object *target = ObjectDB::get_instance(attachment->target);
		for (const auto &entry : attachment->signals) {
			const String session = p_binding.context.session_token;
			const uint64_t identity = attachment->identity;
			const RNSceneSignal signal = entry.value;
			Callable callback(memnew(SceneSignalCallable(attachment->target, [this, session, identity, signal](const Variant **args, int count) {
				Binding *binding = bindings.getptr(session);
				if (!binding || !binding->attachment || binding->attachment->identity != identity || count != signal.arguments.size()) {
					return;
				}
				Dictionary payload;
				for (int i = 0; i < count; ++i) {
					payload[signal.arguments[i]] = *args[i];
				}
				RNError error;
				if (!rn_validate_native_value(payload, signal.payload, error, "signal.payload")) {
					ERR_PRINT(error.describe());
					return;
				}
				publish(*binding, signal.event, rn_apply_native_defaults(payload, signal.payload));
			})));
			if (target->connect(entry.key, callback) != OK) {
				disconnect(p_binding);
				r_error = RNError::make(RNErrorCode::NATIVE, "signal connection failed", "GodotScene.getBinding");
				return false;
			}
			p_binding.connections.push_back({ attachment->target, entry.key, callback });
		}
		publish(p_binding, "ready", Variant());
		return true;
	}
	Binding *binding_for(const RNCallContext &p_context, RNError &r_error) {
		RNSessionRecord session;
		if (!p_context.objects->resolve_session(p_context.session_token, session, r_error)) {
			return nullptr;
		}
		Binding *binding = bindings.getptr(p_context.session_token);
		if (!binding) {
			Binding value;
			value.context = p_context;
			value.root = session.root_view_id;
			bindings[p_context.session_token] = value;
			binding = bindings.getptr(p_context.session_token);
		}
		return refresh(*binding, r_error) ? binding : nullptr;
	}
	RNModuleResult invoke(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context, bool p_async) {
		RNError error;
		Binding *binding = binding_for(p_context, error);
		if (!binding) {
			return RNModuleResult::failure(error);
		}
		if (p_method == "getBinding") {
			return RNModuleResult::success(envelope(*binding, "ready", Variant()));
		}
		if (!binding->attachment || binding->handle != String(p_arguments[1])) {
			return RNModuleResult::failure(RNError::make(RNErrorCode::STALE_HANDLE, "binding is missing or has changed", "GodotScene." + String(p_method)));
		}
		Object *target = p_context.objects->resolve_object(p_arguments[1], p_context.session_token, binding->attachment->capability, error);
		if (!target) {
			return RNModuleResult::failure(error);
		}
		const auto attachment = binding->attachment;
		const String handle = binding->handle;
		StringName method = attachment->snapshot_method;
		RNValueSchema result_schema = binding->attachment->snapshot_schema;
		Array arguments;
		if (p_method != "read") {
			const RNSceneCommand *command = binding->attachment->commands.getptr(StringName(p_arguments[2]));
			if (!command || (command->schema.mode == RNCallMode::ASYNC) != p_async) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "command is not declared in this mode", "GodotScene.call"));
			}
			method = command->method;
			result_schema = command->schema.result;
			const Array supplied = p_arguments[3];
			if (supplied.size() > command->schema.arguments.size()) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "too many command arguments", "GodotScene.call"));
			}
			for (int index = 0; index < command->schema.arguments.size(); ++index) {
				const RNArgumentSchema &argument = command->schema.arguments[index];
				RNValueSchema schema = argument.value;
				schema.nullable = schema.nullable || argument.nullable;
				if (index >= supplied.size() && !argument.optional && !argument.has_default) {
					return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "missing command argument", "GodotScene.call"));
				}
				Variant value = index < supplied.size() ? supplied[index] : argument.has_default ? argument.default_value
																								 : Variant();
				if (!(index >= supplied.size() && argument.optional && !argument.has_default) && !rn_validate_native_value(value, schema, error, "command.arguments")) {
					return RNModuleResult::failure(error);
				}
				if (index >= supplied.size() && argument.optional && !argument.has_default) {
					break;
				}
				arguments.push_back(rn_apply_native_defaults(value, schema));
			}
		}
		Vector<const Variant *> pointers;
		for (int i = 0; i < arguments.size(); ++i) {
			pointers.push_back(&arguments[i]);
		}
		Callable::CallError call_error;
		const Variant value = target->callp(method, pointers.ptrw(), pointers.size(), call_error);
		if (call_error.error != Callable::CallError::CALL_OK) {
			return RNModuleResult::failure(RNError::make(RNErrorCode::NATIVE, "script invocation failed", "GodotScene.call"));
		}
		binding = bindings.getptr(p_context.session_token);
		if (!binding || binding->attachment != attachment || binding->handle != handle || !live_target(attachment->target)) {
			return RNModuleResult::failure(RNError::make(RNErrorCode::STALE_HANDLE, "binding changed during script invocation", "GodotScene.call"));
		}
		if (!rn_validate_native_value(value, result_schema, error, "result")) {
			return RNModuleResult::failure(error);
		}
		if (p_method == "read") {
			return RNModuleResult::success(envelope(*binding, "snapshot", rn_apply_native_defaults(value, result_schema)));
		}
		return RNModuleResult::success(rn_apply_native_defaults(value, result_schema));
	}

public:
	~RNGodotSceneModule() override { shutdown(); }
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context) override { return invoke(p_method, p_arguments, p_context, false); }
	void start_async(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context, const RNCompletionToken &p_completion) override {
		const RNModuleResult result = invoke(p_method, p_arguments, p_context, true);
		if (result.error.is_set()) {
			p_completion.fail(result.error);
		} else {
			p_completion.complete(result.value);
		}
	}
	void on_session_closed(const String &p_session) override {
		Binding *binding = bindings.getptr(p_session);
		if (binding) {
			disconnect(*binding);
			bindings.erase(p_session);
		}
	}
	void on_surface_closed(int p_root_tag, uint64_t p_epoch) override {
		Vector<String> closed;
		for (const auto &entry : bindings) {
			if (entry.value.context.root_tag == p_root_tag && entry.value.context.surface_epoch == p_epoch) {
				closed.push_back(entry.key);
			}
		}
		for (const String &session : closed) {
			on_session_closed(session);
		}
	}
	void on_scene_binding_changed(ObjectID p_root) override {
		for (auto &entry : bindings) {
			if (entry.value.root == p_root) {
				RNError error;
				refresh(entry.value, error);
				if (error.is_set()) {
					ERR_PRINT(error.describe());
				}
			}
		}
	}
	void process_frame(double) override {
		for (auto &entry : bindings) {
			if (entry.value.attachment && !live_target(entry.value.attachment->target)) {
				RNError error;
				refresh(entry.value, error);
			}
		}
	}
	void shutdown() override {
		for (auto &entry : bindings) {
			disconnect(entry.value);
		}
		bindings.clear();
	}
};
} //namespace

bool rn_register_godot_scene_module(RNNativeModuleRegistry &p_registry, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "GodotScene";
	definition.factory = [] { return std::make_unique<RNGodotSceneModule>(); };
	for (const char *name : { "getBinding", "read", "call", "callAsync" }) {
		RNMethodSchema method;
		method.name = name;
		method.requires_session = true;
		method.mode = String(name) == "callAsync" ? RNCallMode::ASYNC : RNCallMode::SYNC;
		method.result = RNValueSchema::value(RNValueType::DYNAMIC);
		RNArgumentSchema session;
		session.name = "session";
		session.value = RNValueSchema::value(RNValueType::SESSION);
		method.arguments.push_back(session);
		if (String(name) != "getBinding") {
			RNArgumentSchema handle;
			handle.name = "handle";
			handle.value = RNValueSchema::value(RNValueType::STRING);
			method.arguments.push_back(handle);
		}
		if (String(name) == "call" || String(name) == "callAsync") {
			RNArgumentSchema command;
			command.name = "command";
			command.value = RNValueSchema::value(RNValueType::STRING);
			method.arguments.push_back(command);
			RNArgumentSchema arguments;
			arguments.name = "arguments";
			arguments.value = RNValueSchema::array(RNValueSchema::value(RNValueType::DYNAMIC));
			method.arguments.push_back(arguments);
		}
		definition.methods.push_back(method);
	}
	RNEventSchema event;
	event.name = "changed";
	event.subscription_name = "onChanged";
	event.requires_session = true;
	event.payload = RNValueSchema::value(RNValueType::DYNAMIC);
	definition.events.push_back(event);
	return p_registry.register_module(definition, r_error);
}
