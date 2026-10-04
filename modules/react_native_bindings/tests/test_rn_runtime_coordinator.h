#pragma once

#include "../root_view/react_native_root_view.h"
#include "../runtime/react_native_runtime_coordinator.h"
#include "../singletons/hermes_runtime_singleton.h"

#include "core/object/callable_mp.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

namespace TestRNRuntimeCoordinator {

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator][SceneTree] idle frame signal disconnects after draining and reconnects on root entry") {
	auto coordinator = ReactNativeRuntimeCoordinator::get_singleton();
	auto runtime = HermesRuntimeSingleton::get_singleton();
	auto tree = SceneTree::get_singleton();
	REQUIRE(coordinator);
	REQUIRE(runtime);
	REQUIRE(tree);
	runtime->reset();
	auto state = coordinator->get_state();
	state->bundle_generation = runtime->get_runtime_generation();
	state->bundle_status = RNBundleStatus::FAILED;
	state->bundle_error = "Fixture has no application bundle.";
	const Callable frame = callable_mp(coordinator, &ReactNativeRuntimeCoordinator::_process_frame);
	for (int turn = 0; turn < 2; ++turn) {
		auto root = memnew(ReactNativeRootView);
		ERR_PRINT_OFF;
		tree->get_root()->add_child(root);
		ERR_PRINT_ON;
		CHECK(tree->is_connected("process_frame", frame));
		runtime->evaluate("globalThis.drained=false;__godotScheduler.setTimeout(()=>{drained=true;},0);undefined;");
		ERR_PRINT_OFF;
		tree->get_root()->remove_child(root);
		ERR_PRINT_ON;
		memdelete(root);
		CHECK(tree->is_connected("process_frame", frame));
		coordinator->_process_frame();
		CHECK(runtime->get_global("drained") == Variant(true));
		CHECK(tree->is_connected("process_frame", frame));
		coordinator->_process_frame();
		CHECK_FALSE(tree->is_connected("process_frame", frame));
	}
	state->bundle_status = RNBundleStatus::UNEVALUATED;
	state->bundle_generation = 0;
	state->bundle_error = String();
	runtime->reset();
}

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
	CHECK(RNSurfaceTagHash{}(left) != RNSurfaceTagHash{}(right));
}

TEST_CASE("[ReactNativeBindings][RuntimeCoordinator] primary touch pointer can capture") {
	RNPointerCaptureProcessor processor;
	RNNativeEvent down;
	down.root_tag = 11;
	down.tag = 42;
	down.name = "topPointerDown";
	down.surface_epoch = 7;
	down.payload["pointerId"] = 0;
	processor.observe(down);
	processor.set_capture(11, 7, 42, 0);
	CHECK(processor.has_capture(11, 7, 42, 0));

	RNNativeEvent move = down;
	move.name = "topPointerMove";
	Vector<RNNativeEvent> gained = processor.apply_pending(move);
	REQUIRE(gained.size() == 1);
	CHECK(gained[0].name == "topGotPointerCapture");
	CHECK(processor.captured_target(11, 7, 0) == 42);
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
