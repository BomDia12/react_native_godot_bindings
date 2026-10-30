#pragma once

#include "rn_schema.h"

#include "core/io/resource.h"

#include <memory>

struct RNContractProperty {
	StringName name;
	RNValueSchema schema;
	Variant default_value;
};

struct RNContractEvent {
	StringName name;
	Vector<RNArgumentSchema> arguments;
};

struct RNCompiledContract {
	String component;
	Vector<RNContractProperty> properties;
	Vector<RNContractEvent> events;
	// A closed record of every property, which is what JavaScript receives.
	RNValueSchema snapshot;

	const RNContractProperty *find_property(const StringName &p_name) const;
};

// What a React component receives from Godot and sends back, declared as a
// Godot resource: typed properties that become inspector properties of the
// root showing the component, and typed events that become its signals.
class RNComponentContract : public Resource {
	GDCLASS(RNComponentContract, Resource);

	String component;
	Array properties;
	Array events;
	Dictionary types;

protected:
	static void _bind_methods();

public:
	void set_component(const String &p_component);
	String get_component() const { return component; }
	void set_properties(const Array &p_properties);
	Array get_properties() const { return properties; }
	void set_events(const Array &p_events);
	Array get_events() const { return events; }
	void set_types(const Dictionary &p_types);
	Dictionary get_types() const { return types; }

	// The value schema of p_definition with named record types replaced by
	// their definitions. p_enclosing lists the named types being resolved
	// around it, which it can't contain again.
	bool resolve_schema(const Dictionary &p_definition, Dictionary &r_resolved, RNError &r_error, const String &p_path, const Vector<String> &p_enclosing = Vector<String>()) const;
	bool compile(RNCompiledContract &r_contract, RNError &r_error) const;
};

// The contract of p_component from <react_native/project/root>/contracts/.
Ref<RNComponentContract> rn_load_contract(const String &p_component);
String rn_contracts_directory();
// Whether this process only generates the contracts' TypeScript and quits.
bool rn_is_generating_contracts();
// The React prop a contract property or event becomes.
String rn_contract_prop_name(const String &p_name);
String rn_contract_event_prop(const String &p_event);
// The value with the record fields it leaves out filled with their schema
// defaults, as binding snapshots send it.
Variant rn_contract_value_with_defaults(const Variant &p_value, const RNValueSchema &p_schema);
