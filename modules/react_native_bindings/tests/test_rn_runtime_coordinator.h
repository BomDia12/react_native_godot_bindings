#pragma once

#include "../runtime/react_native_runtime_coordinator.h"

#include "tests/test_macros.h"

namespace TestRNRuntimeCoordinator {

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator] routed keys keep surface tags distinct") {
	RNSurfaceTag left{ 11, 2 };
	RNSurfaceTag right{ 21, 2 };
	CHECK_FALSE(left == right);
	std::unordered_map<RNSurfaceTag, int, RNSurfaceTagHash> values;
	values[left] = 1;
	values[right] = 2;
	CHECK(values.size() == 2);
	CHECK(values[left] == 1);
	CHECK(values[right] == 2);
}

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator] pointer capture changes on the next pointer event") {
	RNPointerCaptureProcessor processor;
	RNNativeEvent down;
	down.root_tag = 11;
	down.tag = 42;
	down.name = "topPointerDown";
	down.generation = 3;
	down.surface_epoch = 7;
	down.payload["pointerId"] = 5;
	processor.observe(down);
	processor.set_capture(11, 7, 42, 5);
	CHECK(processor.has_capture(11, 7, 42, 5));

	RNNativeEvent move = down;
	move.name = "topPointerMove";
	Vector<RNNativeEvent> gained = processor.apply_pending(move);
	REQUIRE(gained.size() == 1);
	CHECK(gained[0].name == "topGotPointerCapture");
	CHECK(gained[0].tag == 42);
	CHECK(processor.captured_target(11, 7, 5) == 42);

	processor.release_capture(11, 7, 42, 5);
	Vector<RNNativeEvent> lost = processor.apply_pending(move);
	REQUIRE(lost.size() == 1);
	CHECK(lost[0].name == "topLostPointerCapture");
	CHECK(processor.captured_target(11, 7, 5) == 0);
}

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator] stale pointer epochs cannot capture") {
	RNPointerCaptureProcessor processor;
	RNNativeEvent down;
	down.root_tag = 11;
	down.tag = 42;
	down.name = "topPointerDown";
	down.surface_epoch = 7;
	down.payload["pointerId"] = 1;
	processor.observe(down);
	processor.set_capture(11, 8, 42, 1);
	CHECK_FALSE(processor.has_capture(11, 8, 42, 1));
	CHECK(processor.captured_target(11, 8, 1) == 0);
}

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator] removing a captured target emits lost capture") {
	RNPointerCaptureProcessor processor;
	RNNativeEvent down;
	down.root_tag = 11;
	down.tag = 42;
	down.name = "topPointerDown";
	down.generation = 3;
	down.surface_epoch = 7;
	down.payload["pointerId"] = 1;
	processor.observe(down);
	processor.set_capture(11, 7, 42, 1);
	RNNativeEvent move = down;
	move.name = "topPointerMove";
	processor.apply_pending(move);

	RNSurfaceSnapshot snapshot;
	snapshot.root_tag = 11;
	snapshot.runtime_generation = 3;
	snapshot.surface_epoch = 7;
	Vector<RNNativeEvent> events = processor.reconcile_surface(snapshot);
	REQUIRE(events.size() == 1);
	CHECK(events[0].name == "topLostPointerCapture");
	CHECK(events[0].tag == 42);
	CHECK(processor.captured_target(11, 7, 1) == 0);
}

} // namespace TestRNRuntimeCoordinator
