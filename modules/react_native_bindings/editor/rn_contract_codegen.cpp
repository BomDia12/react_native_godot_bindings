#include "rn_contract_codegen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

namespace {

const char *HEADER = "// Generated from the Godot component contracts in %s. Do not edit.\n";

class TypeScriptWriter {
	HashSet<String> godot_types;

	String _base(const Dictionary &p_definition) {
		const String type = p_definition.get("type", "");
		if (type == "boolean" || type == "string") {
			return type;
		}
		if (type == "number" || type == "integer") {
			return "number";
		}
		if (type == "null" || type == "void") {
			return type;
		}
		if (type == "Uint8Array") {
			return "Uint8Array";
		}
		if (type == "named") {
			return p_definition.get("name", "unknown");
		}
		if (type == "array") {
			const String element = type_of(p_definition.get("element", Dictionary()));
			return vformat("readonly %s[]", element.contains(" ") ? "(" + element + ")" : element);
		}
		if (type == "record") {
			return record(p_definition.get("fields", Dictionary()));
		}
		const HashMap<String, String> wrappers = { { "Color", "GodotColor" }, { "Vector2", "GodotVector2" }, { "Vector3", "GodotVector3" }, { "Rect2", "GodotRect2" }, { "Transform2D", "GodotTransform2D" } };
		if (wrappers.has(type)) {
			godot_types.insert(wrappers[type]);
			return wrappers[type];
		}
		return "unknown";
	}

public:
	String type_of(const Dictionary &p_definition) {
		const String base = _base(p_definition);
		return bool(p_definition.get("nullable", false)) ? base + " | null" : base;
	}

	String record(const Dictionary &p_fields) {
		String result = "{";
		for (const KeyValue<Variant, Variant> &field : p_fields) {
			const Dictionary definition = field.value;
			result += vformat(" %s%s: %s;", rn_contract_prop_name(field.key), bool(definition.get("optional", false)) ? "?" : "", type_of(definition));
		}
		return result + (p_fields.is_empty() ? "}" : " }");
	}

	String imports() const {
		Vector<String> names;
		for (const String &name : godot_types) {
			names.push_back(name);
		}
		names.sort();
		return names.is_empty() ? String() : vformat("import type {%s} from 'react-native-godot';\n", String(", ").join(names));
	}
};

bool write_if_changed(const String &p_path, const String &p_content, RNError &r_error) {
	if (FileAccess::exists(p_path) && FileAccess::get_file_as_string(p_path) == p_content) {
		return true;
	}
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null() || !file->store_string(p_content)) {
		r_error = RNError::make(RNErrorCode::NATIVE, vformat("could not write %s", p_path), "contracts.generate");
		return false;
	}
	return true;
}

} // namespace

bool rn_contract_sources(const Vector<Ref<RNComponentContract>> &p_contracts, RNContractSources &r_sources, RNError &r_error) {
	TypeScriptWriter writer;
	HashMap<String, Dictionary> named_types;
	String types;
	String props;
	String map;
	String registrations;
	String imports;
	for (const Ref<RNComponentContract> &contract : p_contracts) {
		RNCompiledContract compiled;
		if (!contract->compile(compiled, r_error)) {
			r_error.message = vformat("%s: %s", contract->get_component(), r_error.message);
			return false;
		}
		for (const KeyValue<Variant, Variant> &entry : contract->get_types()) {
			const String name = entry.key;
			if (named_types.has(name) && named_types[name] != Dictionary(entry.value)) {
				r_error = RNError::make(RNErrorCode::VALIDATION, vformat("contracts declare the type '%s' differently", name), "contracts.generate");
				return false;
			}
			if (!named_types.has(name)) {
				named_types[name] = entry.value;
				types += vformat("export type %s = %s;\n\n", name, writer.record(Dictionary(entry.value).get("fields", Dictionary())));
			}
		}
		const String component = contract->get_component();
		props += vformat("export type %sProps = {\n", component);
		for (const Variant &property : contract->get_properties()) {
			const Dictionary definition = property;
			props += vformat("  %s: %s;\n", rn_contract_prop_name(definition["name"]), writer.type_of(definition["value"]));
		}
		String events;
		for (const Variant &event : contract->get_events()) {
			const Dictionary definition = event;
			Vector<String> arguments;
			for (const Variant &argument : Array(definition.get("arguments", Array()))) {
				const Dictionary argument_definition = argument;
				arguments.push_back(vformat("%s: %s", rn_contract_prop_name(argument_definition["name"]), writer.type_of(argument_definition["value"])));
			}
			props += vformat("  %s: (%s) => void;\n", rn_contract_event_prop(definition["name"]), String(", ").join(arguments));
			events += vformat("%s%s: '%s'", events.is_empty() ? "" : ", ", definition["name"], rn_contract_event_prop(definition["name"]));
		}
		props += "};\n\n";
		map += vformat("  %s: %sProps;\n", component, component);
		imports += vformat("import %s from '../components/%s';\n", component, component);
		registrations += vformat("GodotAppRegistry.registerComponent('%s', () => withGodotContract(%s, {%s}));\n", component, component, events);
	}
	const String header = vformat(HEADER, rn_contracts_directory());
	r_sources.contracts = header + writer.imports() + (writer.imports().is_empty() ? "" : "\n") + types + props + "export type GodotContracts = {\n" + map + "};\n";
	r_sources.registry = p_contracts.is_empty() ? header + "export {};\n" : header + "import {GodotAppRegistry} from 'react-native-godot/app-registry';\nimport {withGodotContract} from 'react-native-godot/contracts';\n" + imports + "\n" + registrations;
	return true;
}

String rn_contract_component_source(const Ref<RNComponentContract> &p_contract) {
	const String component = p_contract->get_component();
	return vformat("import {View} from 'react-native';\nimport type {%sProps} from '../godot/contracts';\n\nexport default function %s(props: %sProps) {\n  return <View />;\n}\n", component, component, component);
}

namespace {

bool load_contracts(Vector<Ref<RNComponentContract>> &r_contracts, RNError &r_error) {
	const String directory = rn_contracts_directory();
	PackedStringArray files = DirAccess::exists(directory) ? DirAccess::get_files_at(directory) : PackedStringArray();
	files.sort();
	for (const String &file : files) {
		if (file.get_extension() == "tres") {
			Ref<RNComponentContract> contract = rn_load_contract(file.get_basename());
			if (contract.is_null()) {
				r_error = RNError::make(RNErrorCode::VALIDATION, vformat("%s is not a contract for the component '%s'", directory.path_join(file), file.get_basename()), "contracts.generate");
				return false;
			}
			r_contracts.push_back(contract);
		}
	}
	return true;
}

bool file_holds(const String &p_path, const String &p_content) {
	return FileAccess::exists(p_path) && FileAccess::get_file_as_string(p_path) == p_content;
}

} // namespace

bool rn_generated_contracts_current() {
	Vector<Ref<RNComponentContract>> contracts;
	RNContractSources sources;
	RNError error;
	// An invalid contract has a warning of its own.
	if (!load_contracts(contracts, error) || !rn_contract_sources(contracts, sources, error)) {
		return true;
	}
	const String root = GLOBAL_GET("react_native/project/root");
	return file_holds(root.path_join("godot/contracts.ts"), sources.contracts) && file_holds(root.path_join("godot/registry.ts"), sources.registry);
}

bool rn_generate_contracts(RNError &r_error) {
	Vector<Ref<RNComponentContract>> contracts;
	RNContractSources sources;
	if (!load_contracts(contracts, r_error) || !rn_contract_sources(contracts, sources, r_error)) {
		return false;
	}
	const String root = GLOBAL_GET("react_native/project/root");
	if (!write_if_changed(root.path_join("godot/contracts.ts"), sources.contracts, r_error) || !write_if_changed(root.path_join("godot/registry.ts"), sources.registry, r_error)) {
		return false;
	}
	for (const Ref<RNComponentContract> &contract : contracts) {
		const String path = root.path_join("components").path_join(contract->get_component() + ".tsx");
		if (!FileAccess::exists(path) && !write_if_changed(path, rn_contract_component_source(contract), r_error)) {
			return false;
		}
	}
	return true;
}
