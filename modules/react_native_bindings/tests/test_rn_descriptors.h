#pragma once

#include "../components/rn_builtin_descriptors.h"
#include "../mounting/rn_mounting_manager.h"
#include "../root_view/react_native_root_view.h"

#include "tests/test_macros.h"

namespace TestRNDescriptors {

struct DescriptorLifecycleStats {
	int prepare_calls = 0;
	int apply_calls = 0;
	int attach_calls = 0;
	int detach_calls = 0;
	int dispose_calls = 0;
	int applied_marker = 0;
};

RNHostTraits lifecycle_traits() {
	RNHostTraits traits;
	traits.container = false;
	traits.measured_leaf = true;
	traits.has_native_children = false;
	traits.emits_layout = false;
	return traits;
}

class LifecycleDescriptor : public RNHostDescriptor {
	std::shared_ptr<DescriptorLifecycleStats> stats;

public:
	explicit LifecycleDescriptor(const std::shared_ptr<DescriptorLifecycleStats> &p_stats) :
			RNHostDescriptor("RCTLifecycleView", lifecycle_traits()), stats(p_stats) {
	}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &) const override {
		stats->prepare_calls++;
		r_state.props = p_node.props.duplicate(true);
		r_state.props["preparedMarker"] = stats->prepare_calls;
		return true;
	}

	Control *create_host(const RNHostContext &) const override {
		return memnew(Control);
	}

	bool apply(Control *, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		stats->apply_calls++;
		stats->applied_marker = p_state.props.get("preparedMarker", 0);
		return true;
	}

	void attach_signals(Control *, const RNHostContext &) const override {
		stats->attach_calls++;
	}

	void detach_signals(Control *, const RNHostContext &) const override {
		stats->detach_calls++;
	}

	void dispose_state(Control *, const RNHostContext &) const override {
		stats->dispose_calls++;
	}
};

Ref<RNShadowNode> descriptor_node(int p_tag, const String &p_name, const std::shared_ptr<const RNHostDescriptor> &p_descriptor = nullptr) {
	Ref<RNShadowNode> node;
	node.instantiate();
	node->tag = p_tag;
	node->root_tag = 11;
	node->runtime_generation = 3;
	node->surface_epoch = 7;
	node->view_name = p_name;
	node->descriptor = p_descriptor;
	if (!node->descriptor) {
		ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton();
		REQUIRE(coordinator != nullptr);
		node->descriptor = coordinator->get_descriptor_registry()->find(p_name);
	}
	return node;
}

Ref<RNShadowNode> descriptor_root(const Vector<Ref<RNShadowNode>> &p_children) {
	Ref<RNShadowNode> root = descriptor_node(11, "RCTRootView");
	root->children = p_children;
	return root;
}

TEST_CASE("[ReactNativeBindings][Descriptors] definitions are exact, duplicate-safe, and frozen") {
	RNHostDescriptorRegistry registry;
	RNError error;
	REQUIRE(rn_register_builtin_descriptors(registry, error));
	CHECK(registry.has("RCTRootView"));
	CHECK(registry.has("RCTView"));
	CHECK(registry.has("RCTText"));
	CHECK(registry.has("RCTRawText"));
	CHECK_FALSE(rn_register_builtin_descriptors(registry, error));
	CHECK(error.code == RNErrorCode::DUPLICATE_REGISTRATION);
	registry.freeze();
	CHECK(registry.is_frozen());
}

TEST_CASE("[ReactNativeBindings][Descriptors][SceneTree] mounting reuses prepared state and rolls back unpublished hosts") {
	auto lifecycle_stats = std::make_shared<DescriptorLifecycleStats>();
	auto descriptor = std::make_shared<LifecycleDescriptor>(lifecycle_stats);
	ReactNativeRootView *owner = memnew(ReactNativeRootView);
	{
		RNMountingManager manager(owner);
		manager.attach(3, 11, 7);
		Ref<RNShadowNode> first_child = descriptor_node(2, "RCTLifecycleView", descriptor);
		Ref<RNShadowNode> first_root = descriptor_root({ first_child });
		RNPendingCommit first_commit;
		first_commit.runtime_generation = 3;
		first_commit.root_tag = 11;
		first_commit.surface_epoch = 7;
		first_commit.revision = 1;
		first_commit.tree = first_root;
		Vector<RNNativeEvent> events;
		String error;
		REQUIRE(manager.commit(first_commit, Size2(320, 200), Transform2D(), events, error));
		CHECK(lifecycle_stats->prepare_calls == 1);
		CHECK(lifecycle_stats->apply_calls == 1);
		CHECK(lifecycle_stats->applied_marker == 1);
		CHECK(lifecycle_stats->attach_calls == 1);

		Ref<RNShadowNode> second_child = descriptor_node(3, "RCTLifecycleView", descriptor);
		Ref<RNShadowNode> second_root = first_root->clone(true, nullptr);
		second_root->children = { first_child, second_child };
		RNPendingCommit second_commit = first_commit;
		second_commit.revision = 2;
		second_commit.tree = second_root;
		manager.set_failure_injection(-1, 0);
		CHECK_FALSE(manager.commit(second_commit, Size2(320, 200), Transform2D(), events, error));
		CHECK(lifecycle_stats->attach_calls == 1);
		CHECK(lifecycle_stats->detach_calls == 0);
		CHECK(lifecycle_stats->dispose_calls == 1);
		CHECK(manager.get_published_revision() == 1);
		CHECK(manager.get_registry().get_node(2) != nullptr);
		CHECK(manager.get_registry().get_node(3) == nullptr);
	}
	memdelete(owner);
}

} // namespace TestRNDescriptors
