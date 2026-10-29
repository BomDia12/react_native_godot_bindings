#include "../editor/rn_contract_codegen.h"
#include "../interop/rn_component_contract.h"
#include "../root_view/react_native_root_view.h"
#include "support/rn_upstream_test_macros.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"

namespace TestRNContracts {

Dictionary schema(const String &p_type) {
	Dictionary definition;
	definition["type"] = p_type;
	return definition;
}

Dictionary property(const String &p_name, const Dictionary &p_value, const Variant &p_default) {
	Dictionary definition;
	definition["name"] = p_name;
	definition["value"] = p_value;
	definition["default"] = p_default;
	return definition;
}

Ref<RNComponentContract> roster_contract() {
	Ref<RNComponentContract> contract;
	contract.instantiate();
	contract->set_component("EnemyRoster");
	Dictionary fields;
	fields["id"] = schema("string");
	fields["max_health"] = schema("integer");
	Dictionary enemy = schema("record");
	enemy["fields"] = fields;
	Dictionary types;
	types["RosterEnemy"] = enemy;
	contract->set_types(types);
	Dictionary named = schema("named");
	named["name"] = "RosterEnemy";
	Dictionary enemies = schema("array");
	enemies["element"] = named;
	Dictionary marked = schema("string");
	marked["nullable"] = true;
	contract->set_properties({ property("enemies", enemies, Array()), property("marked", marked, Variant()), property("tint", schema("Color"), Color(1, 1, 1)) });
	Dictionary argument;
	argument["name"] = "enemy_id";
	argument["value"] = schema("string");
	Dictionary event;
	event["name"] = "mark";
	event["arguments"] = Array({ argument });
	contract->set_events({ event });
	return contract;
}

int contract_changes = 0;

void count_contract_change(const Dictionary &) {
	++contract_changes;
}

String compile_error(const Ref<RNComponentContract> &p_contract) {
	RNCompiledContract compiled;
	RNError error;
	return p_contract->compile(compiled, error) ? String() : error.message;
}

TEST_CASE("[ReactNativeBindings][Contracts] contracts compile with named records into a closed snapshot") {
	RNCompiledContract compiled;
	RNError error;
	REQUIRE(roster_contract()->compile(compiled, error));
	CHECK(compiled.properties.size() == 3);
	CHECK(compiled.properties[0].schema.element->type == RNValueType::RECORD);
	CHECK(compiled.events[0].arguments[0].name == StringName("enemy_id"));
	CHECK(compiled.snapshot.closed);
	CHECK(compiled.snapshot.fields.size() == 3);
}

TEST_CASE("[ReactNativeBindings][Contracts] invalid names, defaults and types are rejected") {
	Ref<RNComponentContract> contract = roster_contract();
	contract->set_component("enemyRoster");
	CHECK(compile_error(contract).contains("PascalCase"));

	for (const char *name : { "maxHealth", "application_key", "reload", "enemies" }) {
		contract = roster_contract();
		Array properties = contract->get_properties();
		properties.push_back(property(name, schema("integer"), 0));
		contract->set_properties(properties);
		CHECK_MESSAGE(!compile_error(contract).is_empty(), name);
	}

	contract = roster_contract();
	contract->set_properties({ property("health", schema("integer"), "full") });
	CHECK(!compile_error(contract).is_empty());

	contract = roster_contract();
	Dictionary missing = schema("named");
	missing["name"] = "Unknown";
	contract->set_properties({ property("enemy", missing, Dictionary()) });
	CHECK(compile_error(contract).contains("Unknown"));

	contract = roster_contract();
	Dictionary itself = schema("named");
	itself["name"] = "Loop";
	Dictionary types;
	types["Loop"] = itself;
	contract->set_types(types);
	contract->set_properties({ property("loop", itself, Dictionary()) });
	CHECK(compile_error(contract).contains("Loop"));
}

TEST_CASE("[ReactNativeBindings][Contracts] TypeScript mirrors contracts in React's naming") {
	RNContractSources sources;
	RNError error;
	REQUIRE(rn_contract_sources({ roster_contract() }, sources, error));
	CHECK(sources.contracts.contains("import type {GodotColor} from 'react-native-godot';"));
	CHECK(sources.contracts.contains("export type RosterEnemy = { id: string; maxHealth: number; };"));
	CHECK(sources.contracts.contains("  enemies: readonly RosterEnemy[];\n  marked: string | null;\n  tint: GodotColor;\n  onMark: (enemyId: string) => void;\n"));
	CHECK(sources.contracts.contains("export type GodotContracts = {\n  EnemyRoster: EnemyRosterProps;\n};\n"));
	CHECK(sources.registry.contains("import EnemyRoster from '../components/EnemyRoster';"));
	CHECK(sources.registry.contains("GodotAppRegistry.registerComponent('EnemyRoster', () => withGodotContract(EnemyRoster, {mark: 'onMark'}));"));
	CHECK(rn_contract_component_source(roster_contract()).contains("export default function EnemyRoster(props: EnemyRosterProps)"));

	REQUIRE(rn_contract_sources({}, sources, error));
	CHECK(sources.registry.ends_with("export {};\n"));
}

TEST_CASE("[ReactNativeBindings][Contracts] a root shows its contract as typed properties and signals") {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const Variant project_root = settings->get_setting("react_native/project/root");
	settings->set_setting("react_native/project/root", "user://rn_contract_test/");
	DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path("user://rn_contract_test/contracts"));
	REQUIRE(ResourceSaver::save(roster_contract(), "user://rn_contract_test/contracts/EnemyRoster.tres") == OK);

	ReactNativeRootView *root = memnew(ReactNativeRootView);
	root->set_contract_values(Dictionary({ { "tint", Color(1, 0, 0) }, { "marked", 3 } }));
	root->set_application_key("EnemyRoster");
	REQUIRE(root->has_contract());
	List<PropertyInfo> properties;
	root->get_property_list(&properties);
	bool listed = false;
	for (const PropertyInfo &info : properties) {
		listed = listed || (info.name == "tint" && info.type == Variant::COLOR);
	}
	CHECK(listed);
	CHECK(Color(root->get("tint")) == Color(1, 0, 0));
	CHECK(root->get("marked").get_type() == Variant::NIL);
	CHECK(root->has_signal("mark"));
	CHECK(Dictionary(root->attach_scene_binding(root, Ref<RNSceneBinding>())).get("code", "") == "E_VALIDATION");

	contract_changes = 0;
	root->connect("contract_changed", callable_mp_static(&count_contract_change));
	root->set("marked", "e1");
	root->set("tint", Color(0, 0, 1));
	ERR_PRINT_OFF;
	root->set("tint", "blue");
	ERR_PRINT_ON;
	CHECK(Color(root->get("tint")) == Color(0, 0, 1));
	MessageQueue::get_singleton()->flush();
	CHECK(contract_changes == 1);

	root->set_application_key("GodotApp");
	CHECK_FALSE(root->has_contract());
	CHECK_FALSE(root->has_signal("mark"));
	memdelete(root);
	settings->set_setting("react_native/project/root", project_root);
}

} // namespace TestRNContracts

void rn_force_link_contract_tests() {}
