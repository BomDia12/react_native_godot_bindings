#include "rn_component_contract.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

namespace {

bool invalid(RNError &r_error, const String &p_message, const String &p_path) {
	r_error = RNError::make(RNErrorCode::VALIDATION, p_message, "contract.compile", p_path);
	return false;
}

bool is_component_name(const String &p_name) {
	if (p_name.is_empty() || !is_ascii_upper_case(p_name[0])) {
		return false;
	}
	for (int i = 0; i < p_name.length(); ++i) {
		if (!is_ascii_alphanumeric_char(p_name[i])) {
			return false;
		}
	}
	return true;
}

bool is_member_name(const String &p_name) {
	if (p_name.is_empty() || !is_ascii_lower_case(p_name[0])) {
		return false;
	}
	for (int i = 0; i < p_name.length(); ++i) {
		if (!is_ascii_lower_case(p_name[i]) && !is_digit(p_name[i]) && p_name[i] != '_') {
			return false;
		}
	}
	return true;
}

// Contract names become properties and signals of the root, so they can't
// reuse a name the root already has.
// Contract values start a surface as plain props, which can't carry int64
// wrappers, byte arrays or Object handles; scene bindings and modules carry
// those. Field names convert between snake_case and camelCase by the schema,
// which dynamic values don't have.
bool carries_plain_values(const RNValueSchema &p_schema) {
	if (p_schema.type == RNValueType::INT64 || p_schema.type == RNValueType::BYTES || p_schema.type == RNValueType::OBJECT || p_schema.type == RNValueType::DYNAMIC || p_schema.type == RNValueType::VOID) {
		return false;
	}
	if (p_schema.element && !carries_plain_values(*p_schema.element)) {
		return false;
	}
	for (const RNRecordFieldSchema &field : p_schema.fields) {
		if (!carries_plain_values(*field.value)) {
			return false;
		}
	}
	return true;
}

bool is_free_on_root(const StringName &p_name) {
	return !ClassDB::has_property("ReactNativeRootView", p_name) && !ClassDB::has_signal("ReactNativeRootView", p_name) && !ClassDB::has_method("ReactNativeRootView", p_name);
}

} // namespace

const RNContractProperty *RNCompiledContract::find_property(const StringName &p_name) const {
	for (const RNContractProperty &property : properties) {
		if (property.name == p_name) {
			return &property;
		}
	}
	return nullptr;
}

void RNComponentContract::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_component", "component"), &RNComponentContract::set_component);
	ClassDB::bind_method(D_METHOD("get_component"), &RNComponentContract::get_component);
	ClassDB::bind_method(D_METHOD("set_properties", "properties"), &RNComponentContract::set_properties);
	ClassDB::bind_method(D_METHOD("get_properties"), &RNComponentContract::get_properties);
	ClassDB::bind_method(D_METHOD("set_events", "events"), &RNComponentContract::set_events);
	ClassDB::bind_method(D_METHOD("get_events"), &RNComponentContract::get_events);
	ClassDB::bind_method(D_METHOD("set_types", "types"), &RNComponentContract::set_types);
	ClassDB::bind_method(D_METHOD("get_types"), &RNComponentContract::get_types);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "component"), "set_component", "get_component");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "properties", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"), "set_properties", "get_properties");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "events", PROPERTY_HINT_ARRAY_TYPE, "Dictionary"), "set_events", "get_events");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "types"), "set_types", "get_types");
}

void RNComponentContract::set_component(const String &p_component) {
	component = p_component;
	emit_changed();
}

void RNComponentContract::set_properties(const Array &p_properties) {
	properties = p_properties;
	emit_changed();
}

void RNComponentContract::set_events(const Array &p_events) {
	events = p_events;
	emit_changed();
}

void RNComponentContract::set_types(const Dictionary &p_types) {
	types = p_types;
	emit_changed();
}

bool RNComponentContract::resolve_schema(const Dictionary &p_definition, Dictionary &r_resolved, RNError &r_error, const String &p_path, const Vector<String> &p_enclosing) const {
	Dictionary resolved = p_definition.duplicate();
	Vector<String> enclosing = p_enclosing;
	while (String(resolved.get("type", "")) == "named") {
		const String type_name = resolved.get("name", "");
		if (enclosing.has(type_name) || types.get(type_name, Variant()).get_type() != Variant::DICTIONARY) {
			return invalid(r_error, vformat("named type '%s' is not declared or contains itself", type_name), p_path);
		}
		enclosing.push_back(type_name);
		// A reference can modify how the type is used, but not its shape, which
		// the generated TypeScript names once.
		Dictionary named = Dictionary(types[type_name]).duplicate();
		for (const KeyValue<Variant, Variant> &entry : resolved) {
			const String key = entry.key;
			if (key == "nullable" || key == "optional" || key == "default") {
				named[key] = entry.value;
			} else if (key != "type" && key != "name") {
				return invalid(r_error, vformat("a reference to '%s' can't set '%s'", type_name, key), p_path);
			}
		}
		resolved = named;
	}
	if (resolved.get("element", Variant()).get_type() == Variant::DICTIONARY) {
		Dictionary element;
		if (!resolve_schema(resolved["element"], element, r_error, p_path + ".element", enclosing)) {
			return false;
		}
		resolved["element"] = element;
	}
	if (resolved.get("fields", Variant()).get_type() == Variant::DICTIONARY) {
		Dictionary fields = Dictionary(resolved["fields"]).duplicate();
		for (const KeyValue<Variant, Variant> &entry : Dictionary(resolved["fields"])) {
			if (!is_member_name(entry.key)) {
				return invalid(r_error, vformat("record field '%s' must be a snake_case name", entry.key), p_path);
			}
			Dictionary field;
			if (entry.value.get_type() != Variant::DICTIONARY || !resolve_schema(entry.value, field, r_error, p_path + "." + String(entry.key), enclosing)) {
				return r_error.is_set() ? false : invalid(r_error, "record field must be a schema", p_path);
			}
			fields[entry.key] = field;
		}
		resolved["fields"] = fields;
	}
	r_resolved = resolved;
	return true;
}

bool RNComponentContract::compile(RNCompiledContract &r_contract, RNError &r_error) const {
	RNCompiledContract compiled;
	compiled.component = component;
	if (!is_component_name(component)) {
		return invalid(r_error, "component must be a PascalCase name", "component");
	}
	HashSet<StringName> names;
	Vector<RNRecordFieldSchema> snapshot_fields;
	for (int i = 0; i < properties.size(); ++i) {
		const String path = vformat("properties[%d]", i);
		const Dictionary definition = properties[i];
		RNContractProperty property;
		property.name = StringName(String(definition.get("name", "")));
		if (!is_member_name(property.name) || names.has(property.name) || !is_free_on_root(property.name)) {
			return invalid(r_error, vformat("'%s' must be a unique snake_case name the root doesn't use", property.name), path + ".name");
		}
		Dictionary schema;
		if (definition.get("value", Variant()).get_type() != Variant::DICTIONARY || !resolve_schema(definition["value"], schema, r_error, path + ".value") || !rn_parse_value_schema(schema, property.schema, r_error, path + ".value")) {
			return r_error.is_set() ? false : invalid(r_error, "property requires a value schema", path + ".value");
		}
		if (!carries_plain_values(property.schema)) {
			return invalid(r_error, "contract values can't be dynamic, void, int64, Uint8Array or Object", path + ".value");
		}
		if (!definition.has("default") || !rn_validate_native_value(definition["default"], property.schema, r_error, path + ".default")) {
			return r_error.is_set() ? false : invalid(r_error, "property requires a default", path + ".default");
		}
		property.default_value = Variant(definition["default"]).duplicate(true);
		names.insert(property.name);
		RNRecordFieldSchema field;
		field.name = property.name;
		field.value = std::make_shared<RNValueSchema>(property.schema);
		field.nullable = property.schema.nullable;
		snapshot_fields.push_back(field);
		compiled.properties.push_back(property);
	}
	for (int i = 0; i < events.size(); ++i) {
		const String path = vformat("events[%d]", i);
		const Dictionary definition = events[i];
		RNContractEvent event;
		event.name = StringName(String(definition.get("name", "")));
		if (!is_member_name(event.name) || names.has(event.name) || !is_free_on_root(event.name)) {
			return invalid(r_error, vformat("'%s' must be a unique snake_case name the root doesn't use", event.name), path + ".name");
		}
		names.insert(event.name);
		const Array arguments = definition.get("arguments", Array());
		HashSet<StringName> argument_names;
		for (int j = 0; j < arguments.size(); ++j) {
			const String argument_path = vformat("%s.arguments[%d]", path, j);
			const Dictionary argument_definition = arguments[j];
			RNArgumentSchema argument;
			argument.name = StringName(String(argument_definition.get("name", "")));
			if (!is_member_name(argument.name) || argument_names.has(argument.name)) {
				return invalid(r_error, "argument names must be unique snake_case names", argument_path + ".name");
			}
			Dictionary schema;
			if (argument_definition.get("value", Variant()).get_type() != Variant::DICTIONARY || !resolve_schema(argument_definition["value"], schema, r_error, argument_path + ".value") || !rn_parse_value_schema(schema, argument.value, r_error, argument_path + ".value")) {
				return r_error.is_set() ? false : invalid(r_error, "argument requires a value schema", argument_path + ".value");
			}
			if (!carries_plain_values(argument.value)) {
				return invalid(r_error, "contract values can't be dynamic, void, int64, Uint8Array or Object", argument_path + ".value");
			}
			argument.nullable = argument.value.nullable;
			argument_names.insert(argument.name);
			event.arguments.push_back(argument);
		}
		compiled.events.push_back(event);
	}
	// Each name becomes a React prop, which must stay distinct and can't be one
	// React or withGodotContract takes for itself.
	HashMap<String, String> react_props = { { "key", String() }, { "ref", String() }, { "rootTag", String() } };
	for (const RNContractProperty &property : compiled.properties) {
		const String prop = rn_contract_prop_name(property.name);
		if (react_props.has(prop)) {
			return invalid(r_error, vformat("'%s' becomes the React prop '%s', which %s", property.name, prop, react_props[prop].is_empty() ? "React reserves" : "'" + react_props[prop] + "' also becomes"), "properties");
		}
		react_props[prop] = property.name;
	}
	for (const RNContractEvent &event : compiled.events) {
		const String prop = rn_contract_event_prop(event.name);
		if (react_props.has(prop)) {
			return invalid(r_error, vformat("'%s' becomes the React prop '%s', which %s", event.name, prop, react_props[prop].is_empty() ? "React reserves" : "'" + react_props[prop] + "' also becomes"), "events");
		}
		react_props[prop] = event.name;
	}
	compiled.snapshot = RNValueSchema::record(snapshot_fields);
	r_contract = std::move(compiled);
	return true;
}

// An underscore before a letter becomes its capital; one before a digit stays,
// so the name converts back to the same snake_case.
String rn_contract_prop_name(const String &p_name) {
	String result;
	for (int i = 0; i < p_name.length(); ++i) {
		if (p_name[i] == '_' && i + 1 < p_name.length() && is_ascii_lower_case(p_name[i + 1])) {
			result += String::chr(p_name[++i]).to_upper();
		} else {
			result += String::chr(p_name[i]);
		}
	}
	return result;
}

String rn_contract_event_prop(const String &p_event) {
	const String name = rn_contract_prop_name(p_event);
	return "on" + name.substr(0, 1).to_upper() + name.substr(1);
}

Variant rn_contract_value_with_defaults(const Variant &p_value, const RNValueSchema &p_schema) {
	if (p_schema.type == RNValueType::RECORD && p_value.get_type() == Variant::DICTIONARY) {
		Dictionary record = Dictionary(p_value).duplicate();
		for (const RNRecordFieldSchema &field : p_schema.fields) {
			const String name = field.name;
			if (record.has(name)) {
				record[name] = rn_contract_value_with_defaults(record[name], *field.value);
			} else if (field.has_default) {
				record[name] = field.default_value.duplicate(true);
			}
		}
		return record;
	}
	if (p_schema.type == RNValueType::ARRAY && p_schema.element && p_value.get_type() == Variant::ARRAY) {
		Array result;
		for (const Variant &item : Array(p_value)) {
			result.push_back(rn_contract_value_with_defaults(item, *p_schema.element));
		}
		return result;
	}
	return p_value;
}

bool rn_is_generating_contracts() {
	return OS::get_singleton()->get_cmdline_args().find("--react-native-generate-contracts") != nullptr;
}

String rn_contracts_directory() {
	return String(GLOBAL_GET("react_native/project/root")).path_join("contracts");
}

Ref<RNComponentContract> rn_load_contract(const String &p_component) {
	const String path = rn_contracts_directory().path_join(p_component + ".tres");
	if (p_component.is_empty() || !ResourceLoader::exists(path)) {
		return Ref<RNComponentContract>();
	}
	Ref<RNComponentContract> contract = ResourceLoader::load(path, "RNComponentContract");
	if (contract.is_valid() && contract->get_component() != p_component) {
		ERR_PRINT(vformat("%s declares the component '%s'; a contract file is named after its component.", path, contract->get_component()));
		return Ref<RNComponentContract>();
	}
	return contract;
}
