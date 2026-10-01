#pragma once

#include "../interop/rn_object_registry.h"
#include "../interop/rn_resource_path.h"
#include "../runtime/react_native_runtime_coordinator.h"

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

TEST_CASE("[ReactNativeBindings][Interop] session and object handles are generation and surface scoped") {
	auto state = std::make_shared<RNRuntimeCoordinatorState>();
	RNSurfaceRoute route;
	route.root_tag = 11;
	route.runtime_generation = 7;
	route.surface_epoch = 3;
	route.status = RNSurfaceStatus::ACTIVE;
	state->routes[11] = route;
	RNObjectRegistry registry(state);
	registry.begin_generation(7);
	RNError error;
	const String session = registry.open_session(11, error);
	REQUIRE_FALSE(session.is_empty());
	Object object;
	const String target = registry.register_object(session, object.get_instance_id(), "Fixture", error);
	CHECK(registry.resolve_object(target, session, "Fixture", error) == &object);
	registry.close_surface(11, 3);
	CHECK(registry.resolve_object(target, session, "Fixture", error) == nullptr);
}

TEST_CASE("[ReactNativeBindings][Interop] handles cannot cross roots or runtime generations") {
	auto state = std::make_shared<RNRuntimeCoordinatorState>();
	RNSurfaceRoute left;
	left.root_tag = 11;
	left.runtime_generation = 4;
	left.surface_epoch = 1;
	left.status = RNSurfaceStatus::ACTIVE;
	state->routes[left.root_tag] = left;
	RNSurfaceRoute right = left;
	right.root_tag = 21;
	state->routes[right.root_tag] = right;
	RNObjectRegistry registry(state);
	registry.begin_generation(4);
	RNError error;
	const String left_session = registry.open_session(left.root_tag, error);
	const String right_session = registry.open_session(right.root_tag, error);
	Object left_target;
	const String handle = registry.register_object(left_session, left_target.get_instance_id(), "Counter", error);
	CHECK(registry.resolve_object(handle, right_session, "Counter", error) == nullptr);
	CHECK(error.code == RNErrorCode::STALE_HANDLE);
	CHECK(registry.resolve_object(handle, left_session, "Other", error) == nullptr);
	CHECK(error.code == RNErrorCode::VALIDATION);

	registry.begin_generation(5);
	CHECK(registry.resolve_object(handle, left_session, "Counter", error) == nullptr);
	CHECK(error.code == RNErrorCode::STALE_HANDLE);
}

TEST_CASE("[ReactNativeBindings][Interop] destroyed objects revoke non-owning handles") {
	auto state = std::make_shared<RNRuntimeCoordinatorState>();
	RNSurfaceRoute route;
	route.root_tag = 11;
	route.runtime_generation = 2;
	route.surface_epoch = 8;
	route.status = RNSurfaceStatus::ACTIVE;
	state->routes[route.root_tag] = route;
	RNObjectRegistry registry(state);
	registry.begin_generation(2);
	RNError error;
	const String session = registry.open_session(route.root_tag, error);
	Object *target = memnew(Object);
	const String handle = registry.register_object(session, target->get_instance_id(), "Counter", error);
	memdelete(target);
	CHECK(registry.resolve_object(handle, session, "Counter", error) == nullptr);
	CHECK(error.code == RNErrorCode::OBJECT_GONE);
}

} // namespace TestRNInterop
