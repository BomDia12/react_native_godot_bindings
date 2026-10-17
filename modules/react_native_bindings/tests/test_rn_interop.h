#pragma once

#include "../interop/rn_object_registry.h"
#include "../interop/rn_resource_path.h"

#include "core/object/object.h"
#include "tests/test_macros.h"

namespace TestRNInterop {

TEST_CASE("[ReactNativeBindings][Interop] local resource paths are exact and confined") {
	String normalized;
	RNError error;
	CHECK(rn_normalize_local_resource_path("res://assets/./icon%2e%2e.png", normalized, error));
	CHECK(normalized == "res://assets/icon%2e%2e.png");
	for (const String &invalid : { String("/tmp/a"), String("file://a"), String("res://../a"), String("user://"), String("res://a\\b") }) {
		error = RNError();
		CHECK_FALSE(rn_normalize_local_resource_path(invalid, normalized, error));
		CHECK(error.code == RNErrorCode::VALIDATION);
	}
}

TEST_CASE("[ReactNativeBindings][Interop] object handles are scoped to their root and close with it") {
	RNObjectRegistry registry;
	RNError error;
	Object object;
	const String target = registry.register_object(11, object.get_instance_id(), "Fixture", error);
	CHECK(registry.resolve_object(target, 11, "Fixture", error) == &object);
	CHECK(registry.register_object(11, object.get_instance_id(), "Fixture", error) == target);
	CHECK(registry.resolve_object(target, 21, "Fixture", error) == nullptr);
	CHECK(error.code == RNErrorCode::STALE_HANDLE);
	CHECK(registry.resolve_object(target, 11, "Other", error) == nullptr);
	CHECK(error.code == RNErrorCode::VALIDATION);
	registry.close_surface(11);
	CHECK(registry.resolve_object(target, 11, "Fixture", error) == nullptr);
	CHECK(error.code == RNErrorCode::STALE_HANDLE);
}

TEST_CASE("[ReactNativeBindings][Interop] destroyed objects revoke non-owning handles") {
	RNObjectRegistry registry;
	RNError error;
	Object *target = memnew(Object);
	const String handle = registry.register_object(11, target->get_instance_id(), "Counter", error);
	memdelete(target);
	CHECK(registry.resolve_object(handle, 11, "Counter", error) == nullptr);
	CHECK(error.code == RNErrorCode::OBJECT_GONE);
}

TEST_CASE("[ReactNativeBindings][Interop] destroyed objects free handle capacity without resolution") {
	RNObjectRegistry registry;
	RNError error;
	for (int i = 0; i < 4096; ++i) {
		Object *transient = memnew(Object);
		CHECK_FALSE(registry.register_object(11, transient->get_instance_id(), "Counter", error).is_empty());
		memdelete(transient);
	}
	CHECK(registry.object_count() == 4096);
	Object live;
	CHECK_FALSE(registry.register_object(11, live.get_instance_id(), "Counter", error).is_empty());
	CHECK(registry.object_count() == 1);
}

} // namespace TestRNInterop
