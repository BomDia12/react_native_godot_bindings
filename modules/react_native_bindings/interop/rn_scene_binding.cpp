#include "rn_scene_binding.h"

#include "core/object/class_db.h"
#include "core/os/thread.h"

namespace {
bool invalid(RNError &r_error, const String &p_message, const String &p_path) {
	r_error = RNError::make(RNErrorCode::VALIDATION, p_message, "scene.attach", p_path);
	return false;
}
bool keys_valid(const Dictionary &p_value, const Vector<String> &p_allowed, RNError &r_error, const String &p_path) {
	if (p_value.size() > 4096) {
		return invalid(r_error, "definition exceeds entry limit", p_path);
	}
	const Array keys = p_value.keys();
	for (const Variant &key : keys) {
		if ((key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) || !p_allowed.has(String(key))) {
			return invalid(r_error, "unknown definition key", p_path);
		}
	}
	return true;
}
bool method_valid(Object *p_target, const StringName &p_method, int p_count, RNError &r_error, const String &p_path) {
	bool valid = false;
	const int count = p_target->get_method_argument_count(p_method, &valid);
	return (valid && count == p_count && !String(p_method).begins_with("_")) || invalid(r_error, "declared method is missing, private or has the wrong arity", p_path);
}
bool compatible(const PropertyInfo &p_info, const RNValueSchema &p_schema) {
	if (p_info.type == Variant::NIL && (p_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
		return true;
	}
	switch (p_schema.type) {
		case RNValueType::VOID:
		case RNValueType::NULL_VALUE:
			return p_info.type == Variant::NIL;
		case RNValueType::DYNAMIC:
			return p_info.type == Variant::NIL;
		case RNValueType::BOOL:
			return p_info.type == Variant::BOOL;
		case RNValueType::FLOAT:
			return p_info.type == Variant::FLOAT;
		case RNValueType::INTEGER:
		case RNValueType::INT64:
			return p_info.type == Variant::INT;
		case RNValueType::STRING:
		case RNValueType::SESSION:
			return p_info.type == Variant::STRING || p_info.type == Variant::STRING_NAME;
		case RNValueType::ARRAY:
			return p_info.type == Variant::ARRAY;
		case RNValueType::RECORD:
			return p_info.type == Variant::DICTIONARY;
		case RNValueType::BYTES:
			return p_info.type == Variant::PACKED_BYTE_ARRAY;
		case RNValueType::COLOR:
			return p_info.type == Variant::COLOR;
		case RNValueType::VECTOR2:
			return p_info.type == Variant::VECTOR2;
		case RNValueType::VECTOR3:
			return p_info.type == Variant::VECTOR3;
		case RNValueType::RECT2:
			return p_info.type == Variant::RECT2;
		case RNValueType::TRANSFORM2D:
			return p_info.type == Variant::TRANSFORM2D;
		case RNValueType::OBJECT:
			return p_info.type == Variant::OBJECT;
	}
	return false;
}
bool argument_compatible(const PropertyInfo &p_info, const RNArgumentSchema &p_schema) {
	const bool variant = p_info.type == Variant::NIL && (p_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT);
	if ((p_schema.nullable || p_schema.value.nullable) && p_info.type != Variant::OBJECT && !variant) {
		return false;
	}
	return compatible(p_info, p_schema.value);
}
bool signature_valid(Object *p_target, const StringName &p_method, const Vector<RNArgumentSchema> &p_arguments, const RNValueSchema &p_result, RNError &r_error) {
	List<MethodInfo> methods;
	p_target->get_method_list(&methods);
	for (const MethodInfo &method : methods) {
		if (method.name != p_method) {
			continue;
		}
		if (method.arguments.size() != p_arguments.size() || !compatible(method.return_val, p_result)) {
			return invalid(r_error, "method signature does not match its schema", "method.signature");
		}
		int index = 0;
		bool optional_seen = false;
		for (const PropertyInfo &argument : method.arguments) {
			if (!argument_compatible(argument, p_arguments[index])) {
				return invalid(r_error, "argument type does not match its schema", "method.signature");
			}
			const bool optional = p_arguments[index].optional || p_arguments[index].has_default;
			if (optional_seen && !optional) {
				return invalid(r_error, "required arguments must precede optional arguments", "method.signature");
			}
			optional_seen = optional_seen || optional;
			if (p_arguments[index].optional && !p_arguments[index].has_default && index < method.arguments.size() - method.default_arguments.size()) {
				return invalid(r_error, "optional argument requires a script or schema default", "method.signature");
			}
			++index;
		}
		return true;
	}
	return invalid(r_error, "method signature is missing", "method.signature");
}
} //namespace

void RNSceneBinding::_bind_methods() {
#define RN_BIND_PROPERTY(m_name, m_type) \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &RNSceneBinding::set_##m_name); \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &RNSceneBinding::get_##m_name); \
	ADD_PROPERTY(PropertyInfo(m_type, #m_name), "set_" #m_name, "get_" #m_name)
	RN_BIND_PROPERTY(capability, Variant::STRING);
	RN_BIND_PROPERTY(schema_version, Variant::INT);
	RN_BIND_PROPERTY(snapshot_method, Variant::STRING_NAME);
	RN_BIND_PROPERTY(snapshot_schema, Variant::DICTIONARY);
	RN_BIND_PROPERTY(commands, Variant::DICTIONARY);
	RN_BIND_PROPERTY(signals, Variant::DICTIONARY);
#undef RN_BIND_PROPERTY
}

bool RNSceneBinding::compile(Object *p_target, RNSceneAttachment &r_attachment, RNError &r_error) const {
	if (!Thread::is_main_thread() || !p_target || capability.is_empty() || schema_version <= 0) {
		return invalid(r_error, "live target, capability and positive version required on main thread", "binding");
	}
	RNSceneAttachment compiled;
	compiled.target = p_target->get_instance_id();
	compiled.capability = capability;
	compiled.schema_version = schema_version;
	compiled.snapshot_method = snapshot_method;
	if (!method_valid(p_target, snapshot_method, 0, r_error, "snapshot_method") || !rn_parse_value_schema(snapshot_schema, compiled.snapshot_schema, r_error, "snapshot_schema")) {
		return false;
	}
	if (!signature_valid(p_target, snapshot_method, {}, compiled.snapshot_schema, r_error)) {
		return false;
	}
	if (commands.size() > 4096 || signals.size() > 4096) {
		return invalid(r_error, "too many commands or signals", "binding");
	}
	const Array operations = commands.keys();
	for (const Variant &operation : operations) {
		if ((operation.get_type() != Variant::STRING && operation.get_type() != Variant::STRING_NAME) || String(operation).is_empty() || commands[operation].get_type() != Variant::DICTIONARY) {
			return invalid(r_error, "invalid command", "commands");
		}
		const Dictionary definition = commands[operation];
		if (!keys_valid(definition, { "method", "mode", "arguments", "result" }, r_error, "commands")) {
			return false;
		}
		if ((definition.get("method", Variant()).get_type() != Variant::STRING && definition.get("method", Variant()).get_type() != Variant::STRING_NAME) || definition.get("arguments", Variant()).get_type() != Variant::ARRAY || definition.get("result", Variant()).get_type() != Variant::DICTIONARY) {
			return invalid(r_error, "command requires method, arguments and result", "commands");
		}
		RNSceneCommand command;
		command.method = StringName(definition["method"]);
		command.schema.name = StringName(operation);
		const String mode = definition.get("mode", "");
		if (mode != "sync" && mode != "queued") {
			return invalid(r_error, "command mode must be sync or queued", "commands.mode");
		}
		command.schema.mode = mode == "sync" ? RNCallMode::SYNC : RNCallMode::ASYNC;
		const Array arguments = definition["arguments"];
		if (arguments.size() > 64 || !method_valid(p_target, command.method, arguments.size(), r_error, "commands.method")) {
			return false;
		}
		for (const Variant &value : arguments) {
			if (value.get_type() != Variant::DICTIONARY) {
				return invalid(r_error, "invalid command argument", "commands.arguments");
			}
			const Dictionary argument = value;
			if (!keys_valid(argument, { "name", "value", "optional", "default", "nullable" }, r_error, "commands.arguments") || argument.get("name", Variant()).get_type() != Variant::STRING || argument.get("value", Variant()).get_type() != Variant::DICTIONARY) {
				return invalid(r_error, "argument requires name and value schema", "commands.arguments");
			}
			for (const char *flag : { "optional", "nullable" }) {
				if (argument.has(flag) && argument[flag].get_type() != Variant::BOOL) {
					return invalid(r_error, "argument flag must be boolean", "commands.arguments");
				}
			}
			RNArgumentSchema schema;
			schema.name = StringName(argument["name"]);
			schema.optional = argument.get("optional", false);
			schema.nullable = argument.get("nullable", false);
			schema.has_default = argument.has("default");
			if (schema.has_default) {
				schema.default_value = argument["default"];
			}
			if (!rn_parse_value_schema(argument["value"], schema.value, r_error, "commands.arguments.value")) {
				return false;
			}
			if (schema.has_default) {
				RNValueSchema default_schema = schema.value;
				default_schema.nullable = default_schema.nullable || schema.nullable;
				if (!rn_validate_native_value(schema.default_value, default_schema, r_error, "commands.arguments.default")) {
					return false;
				}
				schema.default_value = schema.default_value.duplicate(true);
			}
			command.schema.arguments.push_back(schema);
		}
		if (!rn_parse_value_schema(definition["result"], command.schema.result, r_error, "commands.result") || !rn_validate_method_schema(command.schema, r_error)) {
			return false;
		}
		if (!signature_valid(p_target, command.method, command.schema.arguments, command.schema.result, r_error)) {
			return false;
		}
		compiled.commands[command.schema.name] = command;
	}
	const Array signal_names = signals.keys();
	for (const Variant &signal_name : signal_names) {
		if ((signal_name.get_type() != Variant::STRING && signal_name.get_type() != Variant::STRING_NAME) || signals[signal_name].get_type() != Variant::DICTIONARY || !p_target->has_signal(StringName(signal_name))) {
			return invalid(r_error, "declared signal does not exist", "signals");
		}
		const Dictionary definition = signals[signal_name];
		if (!keys_valid(definition, { "event", "arguments", "payload" }, r_error, "signals") || definition.get("event", Variant()).get_type() != Variant::STRING || definition.get("arguments", Variant()).get_type() != Variant::ARRAY || definition.get("payload", Variant()).get_type() != Variant::DICTIONARY) {
			return invalid(r_error, "signal requires event, arguments and payload", "signals");
		}
		RNSceneSignal signal;
		signal.event = StringName(definition["event"]);
		const Array arguments = definition["arguments"];
		List<MethodInfo> native_signals;
		p_target->get_signal_list(&native_signals);
		bool signature_valid = false;
		for (const MethodInfo &info : native_signals) {
			if (info.name == StringName(signal_name)) {
				signature_valid = info.arguments.size() == arguments.size();
			}
		}
		if (!signature_valid || signal.event.is_empty() || !rn_parse_value_schema(definition["payload"], signal.payload, r_error, "signals.payload") || signal.payload.type != RNValueType::RECORD) {
			return invalid(r_error, "signal arity and record payload must match", "signals");
		}
		for (const Variant &argument_name : arguments) {
			if (argument_name.get_type() != Variant::STRING || String(argument_name).is_empty() || signal.arguments.has(StringName(argument_name))) {
				return invalid(r_error, "signal argument names must be unique strings", "signals");
			}
			bool field_found = false;
			for (const RNRecordFieldSchema &field : signal.payload.fields) {
				field_found = field_found || field.name == StringName(argument_name);
			}
			if (!field_found) {
				return invalid(r_error, "signal argument lacks a payload field", "signals");
			}
			signal.arguments.push_back(StringName(argument_name));
		}
		for (const RNRecordFieldSchema &field : signal.payload.fields) {
			if (!signal.arguments.has(field.name) && !field.optional && !field.has_default) {
				return invalid(r_error, "required payload field lacks a signal argument", "signals");
			}
		}
		for (const MethodInfo &info : native_signals) {
			if (info.name != StringName(signal_name)) {
				continue;
			}
			int index = 0;
			for (const PropertyInfo &argument : info.arguments) {
				for (const RNRecordFieldSchema &field : signal.payload.fields) {
					if (field.name == signal.arguments[index] && !compatible(argument, *field.value)) {
						return invalid(r_error, "signal type does not match payload field", "signals.signature");
					}
				}
				++index;
			}
		}
		compiled.signals[StringName(signal_name)] = signal;
	}
	r_attachment = std::move(compiled);
	return true;
}
