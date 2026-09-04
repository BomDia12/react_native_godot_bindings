#pragma once

#include "../singletons/hermes_runtime_singleton.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

namespace TestRNHermesRuntime {

// The module is registered at MODULE_INITIALIZATION_LEVEL_CORE, so the singleton exists
// for the whole test run. Each case resets it so a leftover global cannot mask a failure.
HermesRuntimeSingleton *fresh_runtime() {
	HermesRuntimeSingleton *runtime = HermesRuntimeSingleton::get_singleton();
	if (runtime) {
		runtime->reset();
		runtime->use_filesystem_import_resolver();
	}
	return runtime;
}

String evaluate_string(HermesRuntimeSingleton *p_runtime, const String &p_code) {
	return p_runtime->evaluate(p_code);
}

TEST_CASE("[ReactNativeBindings][HermesRuntime] import is confined to res:// and user://") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);

	const char *escapes[] = {
		"importModule('/etc/passwd')",
		"importModule('../../etc/passwd')",
		"importModule('res://../../etc/passwd')",
		"importModule('')",
	};

	for (const char *code : escapes) {
		ERR_PRINT_OFF;
		runtime->evaluate(String(code));
		ERR_PRINT_ON;
		CHECK_FALSE(runtime->get_last_error().is_empty());
	}
}

// A specifier is arbitrary text from the bundle. Publishing the module factory under that
// name would let importModule('Object') replace a core global.
TEST_CASE("[ReactNativeBindings][HermesRuntime] import does not publish a global named after the specifier") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);

	const String module_path = "user://rn_bindings_import_test.js";
	{
		Ref<FileAccess> file = FileAccess::open(module_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("globalThis.__rn_import_test_ran = true;");
	}

	CHECK(evaluate_string(runtime, vformat("typeof importModule('%s')", module_path)) == "function");
	CHECK(runtime->get_last_error().is_empty());
	CHECK(evaluate_string(runtime, vformat("typeof globalThis['%s']", module_path)) == "undefined");
	CHECK(evaluate_string(runtime, "typeof Object") == "function");
	CHECK(evaluate_string(runtime, "typeof importModule") == "function");

	DirAccess::remove_absolute(module_path);
}

// Pins a known platform limit rather than a target behaviour. Godot's
// String::append_utf8() stops at the first zero byte regardless of the length it is
// given, so a JS string containing a NUL cannot round-trip through Variant. Anything
// depending on binary-safe strings has to carry them as a PackedByteArray instead.
TEST_CASE("[ReactNativeBindings][HermesRuntime] an embedded NUL truncates the converted string") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);

	CHECK(double(runtime->evaluate("'a\\u0000b'.length")) == 3.0);
	CHECK(evaluate_string(runtime, "'a\\u0000b'").length() == 1);
	CHECK(evaluate_string(runtime, "'plain'") == "plain");
	// String::utf8 on both sides: Godot's char * constructor decodes as Latin-1, which
	// would corrupt the source before Hermes ever sees it.
	CHECK(evaluate_string(runtime, String::utf8("'ação ✓'")) == String::utf8("ação ✓"));
}

TEST_CASE("[ReactNativeBindings][HermesRuntime] oversized values are truncated loudly") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);

	// Beyond MAX_OBJECT_PROPERTIES, so conversion drops the tail rather than silently
	// returning a short object.
	ERR_PRINT_OFF;
	const Variant wide = runtime->evaluate(
			"(() => { const o = {}; for (let i = 0; i < 400; ++i) { o['k' + i] = i; } return o; })()");
	ERR_PRINT_ON;
	REQUIRE(wide.get_type() == Variant::DICTIONARY);
	CHECK(Dictionary(wide).size() == 128);
}

} // namespace TestRNHermesRuntime
