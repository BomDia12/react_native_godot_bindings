#ifdef TOOLS_ENABLED

#include "../editor/rn_export_plugin.h"
#include "../editor/rn_source_map.h"
#include "../editor/rn_typescript_watcher.h"
#include "support/rn_upstream_test_macros.h"

namespace TestRNEditor {

TEST_CASE("[ReactNativeBindings][Editor] source maps resolve bundle positions to sources and names") {
	RNSourceMap map;
	REQUIRE(map.parse(R"({"version":3,"sources":["App.tsx","util.ts"],"names":["render","add"],"mappings":"AAAAA;AACA,UCEEC"})"));
	RNSourcePosition position;
	REQUIRE(map.lookup(1, 1, position));
	CHECK(position.source == "App.tsx");
	CHECK(position.line == 1);
	CHECK(position.column == 1);
	CHECK(position.name == "render");
	REQUIRE(map.lookup(2, 5, position));
	CHECK(position.source == "App.tsx");
	CHECK(position.line == 2);
	CHECK(position.name.is_empty());
	REQUIRE(map.lookup(2, 20, position));
	CHECK(position.source == "util.ts");
	CHECK(position.line == 4);
	CHECK(position.column == 3);
	CHECK(position.name == "add");
	CHECK_FALSE(map.lookup(9, 1, position));
}

TEST_CASE("[ReactNativeBindings][Editor] indexed source maps look positions up in their sections") {
	RNSourceMap map;
	REQUIRE(map.parse(R"({"version":3,"sections":[
		{"offset":{"line":0,"column":0},"map":{"version":3,"sources":["a.js"],"names":[],"mappings":"AAAA"}},
		{"offset":{"line":10,"column":4},"map":{"version":3,"sources":["b.js"],"names":[],"mappings":"AAEA"}}]})"));
	RNSourcePosition position;
	REQUIRE(map.lookup(1, 1, position));
	CHECK(position.source == "a.js");
	REQUIRE(map.lookup(11, 5, position));
	CHECK(position.source == "b.js");
	CHECK(position.line == 3);
}

TEST_CASE("[ReactNativeBindings][Editor] TypeScript diagnostics parse into problems") {
	RNProblem problem;
	REQUIRE(rn_parse_typescript_problem("components/App.tsx(3,7): error TS2322: Type 'string' is not assignable to type 'number'.", problem));
	CHECK(problem.file == "components/App.tsx");
	CHECK(problem.line == 3);
	CHECK(problem.column == 7);
	CHECK(problem.message == "error TS2322: Type 'string' is not assignable to type 'number'.");
	CHECK_FALSE(rn_parse_typescript_problem("10:12:01 AM - Starting compilation in watch mode...", problem));
	CHECK(rn_is_typescript_summary("10:12:02 AM - Found 1 error. Watching for file changes."));
	CHECK_FALSE(rn_is_typescript_summary("10:12:01 AM - Starting compilation in watch mode..."));
}

TEST_CASE("[ReactNativeBindings][Editor] exports leave out build listings and JavaScript project configuration") {
	const String root = "res://react_native/";
	const String output = "res://react_native/dist";
	CHECK(RNExportPlugin::is_tooling_file("res://react_native/dist/index.bundle.js.map", root, output));
	CHECK(RNExportPlugin::is_tooling_file("res://react_native/dist/index.bundle.js.dependencies.json", root, output));
	CHECK(RNExportPlugin::is_tooling_file("res://react_native/dist/index.bundle.js.assets-manifest.json", root, output));
	CHECK(RNExportPlugin::is_tooling_file("res://react_native/package.json", root, output));
	CHECK(RNExportPlugin::is_tooling_file("res://package-lock.json", "res://", "res://dist"));
	CHECK_FALSE(RNExportPlugin::is_tooling_file("res://react_native/dist/assets/abc/1.png", root, output));
	CHECK_FALSE(RNExportPlugin::is_tooling_file("res://react_native/contracts/Bar.tres", root, output));
	CHECK_FALSE(RNExportPlugin::is_tooling_file("res://data/package.json", root, output));
}

} // namespace TestRNEditor

#endif // TOOLS_ENABLED

void rn_force_link_editor_tests() {}
