#include "rn_contract_codegen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"

namespace {

const char *HEADER = "// Generated from the Godot component contracts in %s. Do not edit.\n";

String camel_case(const String &p_name) {
	String result;
	bool upper = false;
	for (int i = 0; i < p_name.length(); ++i) {
		if (p_name[i] == '_') {
			upper = true;
			continue;
		}
		result += upper ? String::chr(p_name[i]).to_upper() : String::chr(p_name[i]);
		upper = false;
	}
	return result;
}

String event_prop(const String &p_event) {
	const String name = camel_case(p_event);
	return "on" + name.substr(0, 1).to_upper() + name.substr(1);
}

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
		const HashMap<String, String> wrappers = { { "Color", "GodotColor" }, { "Vector2", "GodotVector2" }, { "Vector3", "GodotVector3" }, { "Rect2", "GodotRect2" }, { "int64", "GodotInt64" }, { "Object", "GodotObject" } };
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
			result += vformat(" %s%s: %s;", camel_case(field.key), bool(definition.get("optional", false)) ? "?" : "", type_of(definition));
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
			props += vformat("  %s: %s;\n", camel_case(definition["name"]), writer.type_of(definition["value"]));
		}
		String events;
		for (const Variant &event : contract->get_events()) {
			const Dictionary definition = event;
			Vector<String> arguments;
			for (const Variant &argument : Array(definition.get("arguments", Array()))) {
				const Dictionary argument_definition = argument;
				arguments.push_back(vformat("%s: %s", camel_case(argument_definition["name"]), writer.type_of(argument_definition["value"])));
			}
			props += vformat("  %s: (%s) => void;\n", event_prop(definition["name"]), String(", ").join(arguments));
			events += vformat("%s%s: '%s'", events.is_empty() ? "" : ", ", definition["name"], event_prop(definition["name"]));
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

bool rn_generate_contracts(RNError &r_error) {
	Vector<Ref<RNComponentContract>> contracts;
	const String directory = rn_contracts_directory();
	PackedStringArray files = DirAccess::exists(directory) ? DirAccess::get_files_at(directory) : PackedStringArray();
	files.sort();
	for (const String &file : files) {
		if (file.get_extension() == "tres") {
			Ref<RNComponentContract> contract = ResourceLoader::load(directory.path_join(file), "RNComponentContract");
			if (contract.is_valid()) {
				contracts.push_back(contract);
			}
		}
	}
	RNContractSources sources;
	if (!rn_contract_sources(contracts, sources, r_error)) {
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
