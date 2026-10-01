#pragma once

#include "../singletons/hermes_runtime_singleton.h"

#include "core/object/callable_mp.h"
#include "core/os/thread.h"
#include "tests/test_macros.h"

#include <atomic>

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

Variant resolve_test_module(const String &p_specifier) {
	(void)p_specifier;
	return String("return 1;");
}

struct ThreadRuntimeProbe {
	HermesRuntimeSingleton *runtime = nullptr;
	std::atomic<bool> returned_ready{ true };

	static void run(void *p_userdata) {
		ThreadRuntimeProbe *probe = static_cast<ThreadRuntimeProbe *>(p_userdata);
		probe->returned_ready.store(probe->runtime->is_ready());
	}
};

TEST_CASE("[ReactNativeBindings][HermesRuntime] bound runtime calls reject Godot worker threads") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);
	const uint64_t generation = runtime->get_runtime_generation();
	ThreadRuntimeProbe probe;
	probe.runtime = runtime;
	Thread thread;
	ERR_PRINT_OFF;
	REQUIRE(thread.start(ThreadRuntimeProbe::run, &probe) != Thread::UNASSIGNED_ID);
	thread.wait_to_finish();
	ERR_PRINT_ON;
	CHECK_FALSE(probe.returned_ready.load());
	CHECK(runtime->get_runtime_generation() == generation);
	CHECK(runtime->is_ready());
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

	runtime->set_import_resolver(callable_mp_static(resolve_test_module));
	CHECK(bool(runtime->evaluate(
			"(() => { const original = Object; const factory = importModule('Object'); "
			"return typeof factory === 'function' && Object === original; })()")));
	CHECK(runtime->get_last_error().is_empty());
	CHECK(evaluate_string(runtime, "typeof importModule") == "function");
}

TEST_CASE("[ReactNativeBindings][HermesRuntime] an embedded NUL rejects the entire conversion") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);

	CHECK(double(runtime->evaluate("'a\\u0000b'.length")) == 3.0);
	ERR_PRINT_OFF;
	CHECK(runtime->evaluate("'a\\u0000b'").get_type() == Variant::NIL);
	ERR_PRINT_ON;
	CHECK(runtime->get_last_error().contains("E_VALIDATION"));
	CHECK(runtime->get_last_error().contains("embedded NUL"));
	CHECK(evaluate_string(runtime, "'plain'") == "plain");
	// String::utf8 on both sides: Godot's char * constructor decodes as Latin-1, which
	// would corrupt the source before Hermes ever sees it.
	CHECK(evaluate_string(runtime, String::utf8("'ação ✓'")) == String::utf8("ação ✓"));
}

TEST_CASE("[ReactNativeBindings][HermesRuntime] oversized values reject without truncation") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);

	const Variant wide = runtime->evaluate(
			"(() => { const o = {}; for (let i = 0; i < 400; ++i) { o['k' + i] = i; } return o; })()");
	REQUIRE(wide.get_type() == Variant::DICTIONARY);
	CHECK(Dictionary(wide).size() == 400);

	ERR_PRINT_OFF;
	CHECK(runtime->evaluate(
						 "(() => { const o = {}; for (let i = 0; i < 4097; ++i) { o['k' + i] = i; } return o; })()")
					.get_type() == Variant::NIL);
	ERR_PRINT_ON;
	CHECK(runtime->get_last_error().contains("E_LIMIT"));
}

TEST_CASE("[ReactNativeBindings][HermesRuntime] callable probing never converts functions") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);
	CHECK_FALSE(runtime->has_global_function("missing"));
	runtime->evaluate("globalThis.probe = 1");
	CHECK_FALSE(runtime->has_global_function("probe"));
	runtime->evaluate("globalThis.probe = () => 42; undefined;");
	CHECK(runtime->has_global_function("probe"));
	CHECK(double(runtime->call_function("probe")) == 42.0);
}

TEST_CASE("[ReactNativeBindings][HermesRuntime] Uint8Array slices copy independently") {
	HermesRuntimeSingleton *runtime = fresh_runtime();
	REQUIRE(runtime != nullptr);
	const Variant value = runtime->evaluate(
			"(() => { const bytes = new Uint8Array([1, 2, 3, 4]); const slice = bytes.subarray(1, 3); "
			"globalThis.mutateBytes = () => { bytes[1] = 9; }; return slice; })()");
	REQUIRE(value.get_type() == Variant::PACKED_BYTE_ARRAY);
	const PackedByteArray bytes = value;
	REQUIRE(bytes.size() == 2);
	CHECK(bytes[0] == 2);
	CHECK(bytes[1] == 3);
	runtime->call_function("mutateBytes");
	CHECK(bytes[0] == 2);
}

} // namespace TestRNHermesRuntime
