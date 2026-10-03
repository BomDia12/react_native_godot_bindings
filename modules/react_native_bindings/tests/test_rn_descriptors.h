#pragma once

#include "../components/rn_builtin_descriptors.h"
#include "../components/rn_text_control.h"
#include "../mounting/rn_mounting_manager.h"
#include "../root_view/react_native_root_view.h"

#include "tests/test_macros.h"

namespace TestRNDescriptors {

TEST_CASE("[ReactNativeBindings][Text][SceneTree] detached native measurement preserves styled UTF16 selection and fixed line boxes") {
	RNHostDescriptorRegistry registry;
	RNError error;
	REQUIRE(rn_register_builtin_descriptors(registry, error));
	Ref<RNShadowNode> text;
	text.instantiate();
	text->view_name = "RCTText";
	text->tag = 2;
	text->descriptor = registry.find("RCTText");
	text->props["fontSize"] = 20;
	text->props["selectable"] = true;
	text->props["lineHeight"] = 30;
	text->props["numberOfLines"] = 1.0;
	Ref<RNShadowNode> raw;
	raw.instantiate();
	raw->view_name = "RCTRawText";
	raw->descriptor = registry.find("RCTRawText");
	raw->props["text"] = String::utf8("A😀é native text");
	text->children.push_back(raw);
	RNPreparedHostState prepared;
	REQUIRE(text->descriptor->prepare(*text.ptr(), prepared, error));
	REQUIRE(text->descriptor->resolve_resources(prepared, RNHostContext(), error));
	const auto document = std::static_pointer_cast<const RNTextDocument>(prepared.component_data);
	CHECK(document->utf16_at(2) == 3);
	CHECK(document->character_at(2) == 1);
	CHECK(document->character_at(3) == 2);
	RNMeasureConstraints constraints;
	constraints.width = 300;
	constraints.width_mode = RNMeasureMode::AT_MOST;
	const Size2 measured = text->descriptor->measure(prepared, constraints);
	CHECK(measured.x > 0);
	CHECK(measured.y == 30);
	RNTextControl *native = memnew(RNTextControl);
	native->set_external_layout_enabled(true);
	native->apply_document(document);
	native->set_size(Size2(300, 30));
	REQUIRE(native->validate_detached_layout());
	CHECK(native->get_content_height() == measured.y);
	native->set_selection_range(1, 4);
	CHECK(native->get_selection_from() == 1);
	CHECK(native->get_selection_to() == 3);
	const Variant retained = text->descriptor->capture_state(native);
	Ref<RNShadowNode> changed = text->clone(false, nullptr);
	changed->props["fontSize"] = 26;
	RNPreparedHostState changed_state;
	REQUIRE(text->descriptor->prepare(*changed.ptr(), changed_state, error));
	REQUIRE(text->descriptor->resolve_resources(changed_state, RNHostContext(), error));
	REQUIRE(text->descriptor->apply(native, changed_state, RNHostContext(), error));
	CHECK(native->get_selection_from() == 1);
	CHECK(native->get_selection_to() == 3);
	text->descriptor->restore_state(native, retained);
	CHECK(bool(native->get_document() == document));
	CHECK(native->get_selection_from() == 1);
	CHECK(native->get_selection_to() == 3);
	memdelete(native);
}

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

class StatefulContainer : public Control {
public:
	Control *content = nullptr;
	int offset = 0;

	StatefulContainer() {
		content = memnew(Control);
		add_child(content);
		content->add_child(memnew(Control));
	}
};

class StatefulDescriptor : public RNHostDescriptor {
public:
	mutable int published = 0;
	mutable uint64_t dependency = 1;

	StatefulDescriptor() : RNHostDescriptor("RCTStatefulContainer", RNHostTraits()) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		r_state.dependency_revision = dependency;
		return true;
	}

	Control *create_host(const RNHostContext &) const override {
		return memnew(StatefulContainer);
	}

	Control *get_child_container(Control *p_host, const RNHostContext &) const override {
		return static_cast<StatefulContainer *>(p_host)->content;
	}

	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &r_error) const override {
		auto *host = static_cast<StatefulContainer *>(p_host);
		host->offset = p_state.props.get("offset", 0);
		if (bool(p_state.props.get("reject", false))) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "rejected state", "stateful.apply");
			return false;
		}
		return true;
	}

	Variant capture_state(Control *p_host) const override {
		return static_cast<StatefulContainer *>(p_host)->offset;
	}

	void restore_state(Control *p_host, const Variant &p_state) const override {
		static_cast<StatefulContainer *>(p_host)->offset = p_state;
	}

	void after_publish(Control *, const RNPreparedHostState &, const RNHostContext &) const override {
		published++;
	}
};

TEST_CASE("[ReactNativeBindings][Descriptors][SceneTree] internal containers preserve authored children and rollback native state before effects") {
	auto descriptor = std::make_shared<StatefulDescriptor>();
	ReactNativeRootView *owner = memnew(ReactNativeRootView);
	{
		RNMountingManager manager(owner);
		manager.attach(3, 11, 7);
		Ref<RNShadowNode> container = descriptor_node(2, "RCTStatefulContainer", descriptor);
		container->props["offset"] = 12;
		container->children = { descriptor_node(3, "RCTView"), descriptor_node(4, "RCTView") };
		RNPendingCommit commit;
		commit.runtime_generation = 3;
		commit.root_tag = 11;
		commit.surface_epoch = 7;
		commit.revision = 1;
		commit.tree = descriptor_root({ container });
		Vector<RNNativeEvent> events;
		String error;
		REQUIRE(manager.commit(commit, Size2(320, 200), Transform2D(), events, error));
		auto *host = static_cast<StatefulContainer *>(manager.get_registry().get_node(2));
		const ObjectID authored = host->content->get_child(0)->get_instance_id();
		CHECK(manager.get_registry().get_node(3)->get_parent() == host->content);
		CHECK(manager.get_registry().get_node(4)->get_parent() == host->content);
		CHECK(descriptor->published == 0);
		manager.activate_published_hosts();
		manager.activate_published_hosts();
		CHECK(descriptor->published == 1);
		host->offset = 27;

		Ref<RNShadowNode> rejected = container->clone(false, nullptr);
		rejected->props["offset"] = 99;
		rejected->props["reject"] = true;
		commit.revision = 2;
		commit.tree = descriptor_root({ rejected });
		CHECK_FALSE(manager.commit(commit, Size2(320, 200), Transform2D(), events, error));
		manager.activate_published_hosts();
		CHECK(host->offset == 27);
		CHECK(descriptor->published == 1);
		CHECK(manager.get_published_revision() == 1);
		CHECK(host->content->get_child(0)->get_instance_id() == authored);

		Ref<RNShadowNode> reordered = container->clone(true, nullptr);
		reordered->children = { container->children[1], container->children[0] };
		commit.revision = 3;
		commit.tree = descriptor_root({ reordered });
		REQUIRE(manager.commit(commit, Size2(320, 200), Transform2D(), events, error));
		CHECK(host->content->get_child(0)->get_instance_id() == authored);
		CHECK(host->content->get_child(1) == manager.get_registry().get_node(4));
		CHECK(host->content->get_child(2) == manager.get_registry().get_node(3));
		manager.activate_published_hosts();
		CHECK(descriptor->published == 2);
	}
	memdelete(owner);
}

class DependencyDescriptor : public RNHostDescriptor {
public:
	mutable uint64_t dependency = 1;
	DependencyDescriptor() : RNHostDescriptor("RCTDependency", lifecycle_traits()) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		RNHostDescriptor::prepare(p_node, r_state, r_error);
		r_state.dependency_revision = dependency;
		return true;
	}

	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &) const override {
		return Size2(20, p_state.dependency_revision * 10);
	}
};

TEST_CASE("[ReactNativeBindings][Descriptors] identical roots remeasure changed immutable dependencies and restore prepared data") {
	auto descriptor = std::make_shared<DependencyDescriptor>();
	Ref<RNShadowNode> child = descriptor_node(2, "RCTDependency", descriptor);
	Ref<RNShadowNode> root = descriptor_root({ child });
	RNLayoutTree layout;
	HashMap<int, RNPreparedHostState> states;
	RNError native_error;
	String error;
	HashMap<int, Rect2> boxes;
	REQUIRE(root->descriptor->prepare(*root.ptr(), states[11], native_error));
	REQUIRE(descriptor->prepare(*child.ptr(), states[2], native_error));
	REQUIRE(layout.prepare(root, Size2(320, 200), boxes, error, &states));
	layout.publish();
	CHECK(boxes[2].size.y == 10);
	HashMap<int, RNPreparedHostState> retained;
	for (const KeyValue<int, RNPreparedHostState> &entry : states) {
		retained[entry.key] = entry.value;
	}
	descriptor->dependency = 2;
	REQUIRE(descriptor->prepare(*child.ptr(), states[2], native_error));
	REQUIRE(layout.prepare(root, Size2(320, 200), boxes, error, &states));
	CHECK(boxes[2].size.y == 20);
	REQUIRE(layout.rebuild(root, Size2(320, 200), error, &retained));
	REQUIRE(layout.prepare(root, Size2(320, 200), boxes, error, &retained));
	CHECK(boxes[2].size.y == 10);
}

} // namespace TestRNDescriptors
