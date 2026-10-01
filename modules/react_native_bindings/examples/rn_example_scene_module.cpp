#include "rn_example_scene_module.h"

#include "../root_view/react_native_root_view.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/object.h"

namespace {

HashMap<ObjectID, ObjectID> counters_by_root;

RNRecordFieldSchema field(const StringName &p_name, const RNValueSchema &p_schema) {
	RNRecordFieldSchema result;
	result.name = p_name;
	result.value = std::make_shared<RNValueSchema>(p_schema);
	return result;
}

RNArgumentSchema argument(const StringName &p_name, const RNValueSchema &p_schema) {
	RNArgumentSchema result;
	result.name = p_name;
	result.value = p_schema;
	return result;
}

RNValueSchema counter_value_schema() {
	Vector<RNRecordFieldSchema> fields;
	fields.push_back(field("value", RNValueSchema::value(RNValueType::FLOAT)));
	fields.push_back(field("position", RNValueSchema::value(RNValueType::VECTOR2)));
	fields.push_back(field("tint", RNValueSchema::value(RNValueType::COLOR)));
	return RNValueSchema::record(fields);
}

RNValueSchema echo_schema() {
	Vector<RNRecordFieldSchema> fields;
	fields.push_back(field("color", RNValueSchema::value(RNValueType::COLOR)));
	fields.push_back(field("position", RNValueSchema::value(RNValueType::VECTOR2)));
	fields.push_back(field("integer", RNValueSchema::value(RNValueType::INT64)));
	fields.push_back(field("bytes", RNValueSchema::value(RNValueType::BYTES)));
	return RNValueSchema::record(fields);
}

class RNExampleSignalBridge : public Object {
	std::weak_ptr<RNNativeModuleRegistry> registry;
	uint64_t generation = 0;

public:
	void configure(const std::weak_ptr<RNNativeModuleRegistry> &p_registry, uint64_t p_generation) {
		registry = p_registry;
		generation = p_generation;
	}

	void changed(const Dictionary &p_payload, const String &p_session) {
		if (std::shared_ptr<RNNativeModuleRegistry> owner = registry.lock()) {
			owner->queue_event("ExampleScene", "changed", p_session, generation, p_payload);
		}
	}
};

class RNExampleSceneModule : public RNNativeModule {
	struct SignalConnection {
		ObjectID counter_id;
		String session;
		Callable callable;
	};

	RNExampleSignalBridge *signal_bridge = nullptr;
	Vector<SignalConnection> signal_connections;

	void connect_counter(RNExampleCounter *p_counter, const RNCallContext &p_context) {
		if (!p_counter) {
			return;
		}
		for (const SignalConnection &connection : signal_connections) {
			if (connection.counter_id == p_counter->get_instance_id() && connection.session == p_context.session_token) {
				return;
			}
		}
		signal_bridge->configure(p_context.registry, p_context.generation);
		const Callable callback = callable_mp(signal_bridge, &RNExampleSignalBridge::changed).bind(p_context.session_token);
		p_counter->connect("changed", callback);
		signal_connections.push_back({ p_counter->get_instance_id(), p_context.session_token, callback });
	}

	RNExampleCounter *counter(const Array &p_arguments, const RNCallContext &p_context, RNError &r_error) const {
		if (p_arguments.size() < 2 || p_arguments[1].get_type() != Variant::STRING) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "counter target is required", "ExampleScene.target", "args.target");
			return nullptr;
		}
		Object *object = p_context.objects->resolve_object(p_arguments[1], p_context.session_token, "ExampleCounter", r_error);
		return Object::cast_to<RNExampleCounter>(object);
	}

public:
	RNExampleSceneModule() {
		signal_bridge = memnew(RNExampleSignalBridge);
	}

	~RNExampleSceneModule() override {
		for (const SignalConnection &connection : signal_connections) {
			RNExampleCounter *counter = Object::cast_to<RNExampleCounter>(ObjectDB::get_instance(connection.counter_id));
			if (counter && counter->is_connected("changed", connection.callable)) {
				counter->disconnect("changed", connection.callable);
			}
		}
		memdelete(signal_bridge);
	}

	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context) override {
		if (p_method == "getConstants") {
			Dictionary constants;
			constants["name"] = "ExampleScene";
			constants["version"] = int64_t(1);
			return RNModuleResult::success(constants);
		}
		if (p_method == "echo") {
			return RNModuleResult::success(p_arguments[0]);
		}
		if (p_method == "getTarget") {
			RNSessionRecord session;
			RNError error;
			if (!p_context.objects->resolve_session(p_context.session_token, session, error)) {
				return RNModuleResult::failure(error);
			}
			const ObjectID *counter_id = counters_by_root.getptr(session.root_view_id);
			if (!counter_id || !ObjectDB::get_instance(*counter_id)) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::OBJECT_GONE, "this root has no registered RNExampleCounter", "ExampleScene.getTarget", "result"));
			}
			String handle = p_context.objects->register_object(p_context.session_token, *counter_id, "ExampleCounter", error);
			if (!error.is_set()) {
				connect_counter(Object::cast_to<RNExampleCounter>(ObjectDB::get_instance(*counter_id)), p_context);
			}
			return error.is_set() ? RNModuleResult::failure(error) : RNModuleResult::success(handle);
		}
		RNError error;
		RNExampleCounter *target = counter(p_arguments, p_context, error);
		if (!target) {
			return RNModuleResult::failure(error);
		}
		if (p_method == "read") {
			return RNModuleResult::success(target->read());
		}
		return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "unknown ExampleScene method", "ExampleScene." + String(p_method)));
	}

	void start_async(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context, const RNCompletionToken &p_completion) override {
		if (p_method != "incrementLater") {
			RNNativeModule::start_async(p_method, p_arguments, p_context, p_completion);
			return;
		}
		RNError error;
		RNExampleCounter *target = counter(p_arguments, p_context, error);
		if (!target) {
			p_completion.fail(error);
			return;
		}
		Dictionary result = target->increment(double(p_arguments[2]));
		p_completion.complete(result);
	}
};

RNMethodSchema method(const StringName &p_name, const RNValueSchema &p_result, RNCallMode p_mode = RNCallMode::SYNC, bool p_session = false) {
	RNMethodSchema result;
	result.name = p_name;
	result.result = p_result;
	result.mode = p_mode;
	result.requires_session = p_session;
	return result;
}

} // namespace

void RNExampleCounter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_view_path", "path"), &RNExampleCounter::set_root_view_path);
	ClassDB::bind_method(D_METHOD("get_root_view_path"), &RNExampleCounter::get_root_view_path);
	ClassDB::bind_method(D_METHOD("read"), &RNExampleCounter::read);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_view_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "ReactNativeRootView"), "set_root_view_path", "get_root_view_path");
	ADD_SIGNAL(MethodInfo("changed", PropertyInfo(Variant::DICTIONARY, "value")));
}

void RNExampleCounter::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(get_node_or_null(root_view_path));
		if (root) {
			counters_by_root[root->get_instance_id()] = get_instance_id();
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		Vector<ObjectID> roots;
		for (const KeyValue<ObjectID, ObjectID> &entry : counters_by_root) {
			if (entry.value == get_instance_id()) {
				roots.push_back(entry.key);
			}
		}
		for (ObjectID root : roots) {
			counters_by_root.erase(root);
		}
		if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
			coordinator->get_native_module_registry()->get_objects().unregister_object(get_instance_id());
		}
	}
}

Dictionary RNExampleCounter::read() const {
	Dictionary result;
	result["value"] = value;
	result["position"] = position;
	result["tint"] = tint;
	return result;
}

Dictionary RNExampleCounter::increment(double p_amount) {
	value += p_amount;
	position.x += p_amount;
	Dictionary result = read();
	emit_signal("changed", result);
	return result;
}

bool rn_register_example_scene_module(RNNativeModuleRegistry &p_registry, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "ExampleScene";
	Vector<RNRecordFieldSchema> constant_fields;
	constant_fields.push_back(field("name", RNValueSchema::value(RNValueType::STRING)));
	constant_fields.push_back(field("version", RNValueSchema::value(RNValueType::INTEGER)));
	RNMethodSchema constants = method("getConstants", RNValueSchema::record(constant_fields));
	definition.methods.push_back(constants);
	RNMethodSchema echo = method("echo", echo_schema());
	echo.arguments.push_back(argument("value", echo_schema()));
	definition.methods.push_back(echo);
	RNMethodSchema target = method("getTarget", RNValueSchema::value(RNValueType::OBJECT), RNCallMode::SYNC, true);
	target.arguments.push_back(argument("session", RNValueSchema::value(RNValueType::SESSION)));
	target.result.capability = "ExampleCounter";
	definition.methods.push_back(target);
	RNMethodSchema read = method("read", counter_value_schema(), RNCallMode::SYNC, true);
	read.arguments.push_back(argument("session", RNValueSchema::value(RNValueType::SESSION)));
	RNValueSchema target_schema = RNValueSchema::value(RNValueType::OBJECT);
	target_schema.capability = "ExampleCounter";
	read.arguments.push_back(argument("target", target_schema));
	definition.methods.push_back(read);
	RNMethodSchema increment = method("incrementLater", counter_value_schema(), RNCallMode::ASYNC, true);
	increment.arguments.push_back(argument("session", RNValueSchema::value(RNValueType::SESSION)));
	increment.arguments.push_back(argument("target", target_schema));
	increment.arguments.push_back(argument("amount", RNValueSchema::value(RNValueType::FLOAT)));
	definition.methods.push_back(increment);
	RNEventSchema changed;
	changed.name = "changed";
	changed.subscription_name = "onChanged";
	changed.payload = counter_value_schema();
	changed.requires_session = true;
	definition.events.push_back(changed);
	definition.factory = []() { return std::make_unique<RNExampleSceneModule>(); };
	return p_registry.register_module(definition, r_error);
}
