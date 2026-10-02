#pragma once

#include "../components/rn_host_descriptor_registry.h"
#include "../mounting/rn_layout_tree.h"
#include "../mounting/rn_tree_differ.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "tests/test_macros.h"

namespace TestRNMounting {

Ref<RNShadowNode> mounting_node(int p_tag, const String &p_name, const Dictionary &p_props = Dictionary()) {
	Ref<RNShadowNode> node;
	node.instantiate();
	node->tag = p_tag;
	node->root_tag = 11;
	node->runtime_generation = 3;
	node->surface_epoch = 7;
	node->view_name = p_name;
	if (ReactNativeRuntimeCoordinator *coordinator = ReactNativeRuntimeCoordinator::get_singleton()) {
		node->descriptor = coordinator->get_descriptor_registry()->find(p_name);
	}
	node->props = p_props;
	return node;
}

Ref<RNShadowNode> mounting_root(const Vector<Ref<RNShadowNode>> &p_children) {
	Ref<RNShadowNode> root = mounting_node(11, "RCTRootView");
	root->children = p_children;
	return root;
}

Ref<RNShadowNode> clone_with_children(const Ref<RNShadowNode> &p_node, const Vector<Ref<RNShadowNode>> &p_children) {
	Ref<RNShadowNode> clone = p_node->clone(true, nullptr);
	clone->children = p_children;
	return clone;
}

TEST_CASE("[ReactNativeBindings][Mounting] identical shared roots skip descendant diff work") {
	Ref<RNShadowNode> leaf = mounting_node(3, "RCTText");
	Ref<RNShadowNode> view = mounting_node(2, "RCTView");
	view->children.push_back(leaf);
	Ref<RNShadowNode> root = mounting_root({ view });
	Vector<RNMountingMutation> mutations;
	String error;
	RNTreeDifferStats stats;
	CHECK(RNTreeDiffer::diff(root, root, mutations, error, &stats));
	CHECK(mutations.is_empty());
	CHECK(stats.visited_nodes == 0);
	CHECK(stats.child_maps_allocated == 0);
}

TEST_CASE("[ReactNativeBindings][Mounting] leaf updates preserve the host mutation identity") {
	Dictionary old_props;
	old_props["opacity"] = 1.0;
	Dictionary new_props;
	new_props["opacity"] = 0.5;
	Ref<RNShadowNode> old_leaf = mounting_node(2, "RCTView", old_props);
	Ref<RNShadowNode> new_leaf = old_leaf->clone(false, &new_props);
	Ref<RNShadowNode> old_root = mounting_root({ old_leaf });
	Ref<RNShadowNode> new_root = clone_with_children(old_root, { new_leaf });
	Vector<RNMountingMutation> mutations;
	String error;
	REQUIRE(RNTreeDiffer::diff(old_root, new_root, mutations, error));
	REQUIRE(mutations.size() == 1);
	CHECK(mutations[0].type == RNMutationType::UPDATE);
	CHECK(mutations[0].old_node->tag == 2);
	CHECK(mutations[0].new_node->tag == 2);
}

TEST_CASE("[ReactNativeBindings][Mounting] first mount creates hosts before inserting the final hierarchy") {
	Ref<RNShadowNode> child = mounting_node(3, "RCTText");
	Ref<RNShadowNode> parent = mounting_node(2, "RCTView");
	parent->children.push_back(child);
	Ref<RNShadowNode> root = mounting_root({ parent });
	Vector<RNMountingMutation> mutations;
	String error;
	REQUIRE(RNTreeDiffer::diff(Ref<RNShadowNode>(), root, mutations, error));
	REQUIRE(mutations.size() == 4);
	CHECK(mutations[0].type == RNMutationType::CREATE);
	CHECK(mutations[0].new_node->tag == 2);
	CHECK(mutations[1].type == RNMutationType::CREATE);
	CHECK(mutations[1].new_node->tag == 3);
	CHECK(mutations[2].type == RNMutationType::INSERT);
	CHECK(mutations[2].parent_tag == 11);
	CHECK(mutations[2].index == 0);
	CHECK(mutations[3].type == RNMutationType::INSERT);
	CHECK(mutations[3].parent_tag == 2);
	CHECK(mutations[3].index == 0);
}

TEST_CASE("[ReactNativeBindings][Mounting] sibling reorder is remove then insert for one retained tag") {
	Ref<RNShadowNode> first = mounting_node(2, "RCTView");
	Ref<RNShadowNode> second = mounting_node(3, "RCTView");
	Ref<RNShadowNode> old_root = mounting_root({ first, second });
	Ref<RNShadowNode> new_root = clone_with_children(old_root, { second, first });
	Vector<RNMountingMutation> mutations;
	String error;
	REQUIRE(RNTreeDiffer::diff(old_root, new_root, mutations, error));
	REQUIRE(mutations.size() == 2);
	CHECK(mutations[0].type == RNMutationType::REMOVE);
	CHECK(mutations[0].old_node->tag == 3);
	CHECK(mutations[0].parent_tag == 11);
	CHECK(mutations[0].index == 1);
	CHECK(mutations[1].type == RNMutationType::INSERT);
	CHECK(mutations[1].new_node->tag == 3);
	CHECK(mutations[1].parent_tag == 11);
	CHECK(mutations[1].index == 0);
}

TEST_CASE("[ReactNativeBindings][Mounting] retained parent changes and duplicate tags are rejected") {
	Ref<RNShadowNode> leaf = mounting_node(4, "RCTView");
	Ref<RNShadowNode> old_parent = mounting_node(2, "RCTView");
	old_parent->children.push_back(leaf);
	Ref<RNShadowNode> new_parent = mounting_node(3, "RCTView");
	new_parent->children.push_back(leaf);
	Ref<RNShadowNode> old_root = mounting_root({ old_parent, new_parent->clone(true, nullptr) });
	Ref<RNShadowNode> new_root = mounting_root({ old_parent->clone(true, nullptr), new_parent });
	Vector<RNMountingMutation> mutations;
	String error;
	CHECK_FALSE(RNTreeDiffer::diff(old_root, new_root, mutations, error));
	CHECK(error.contains("changed logical parent"));

	Ref<RNShadowNode> duplicate_root = mounting_root({ leaf, leaf });
	error = String();
	CHECK_FALSE(RNTreeDiffer::diff(Ref<RNShadowNode>(), duplicate_root, mutations, error));
	const bool duplicate_reported = error.contains("repeated") || error.contains("duplicate");
	CHECK(duplicate_reported);
}

TEST_CASE("[ReactNativeBindings][Mounting] subtree deletion removes descendants before ancestors") {
	Ref<RNShadowNode> child = mounting_node(3, "RCTText");
	Ref<RNShadowNode> parent = mounting_node(2, "RCTView");
	parent->children.push_back(child);
	Ref<RNShadowNode> old_root = mounting_root({ parent });
	Ref<RNShadowNode> new_root = clone_with_children(old_root, {});
	Vector<RNMountingMutation> mutations;
	String error;
	REQUIRE(RNTreeDiffer::diff(old_root, new_root, mutations, error));
	REQUIRE(mutations.size() == 4);
	CHECK(mutations[0].type == RNMutationType::REMOVE);
	CHECK(mutations[0].old_node->tag == 3);
	CHECK(mutations[1].type == RNMutationType::REMOVE);
	CHECK(mutations[1].old_node->tag == 2);
	CHECK(mutations[2].type == RNMutationType::DELETE);
	CHECK(mutations[2].old_node->tag == 3);
	CHECK(mutations[3].type == RNMutationType::DELETE);
	CHECK(mutations[3].old_node->tag == 2);
}

TEST_CASE("[ReactNativeBindings][Mounting] host type and cross-surface retention are rejected") {
	Ref<RNShadowNode> old_view = mounting_node(2, "RCTView");
	Ref<RNShadowNode> old_root = mounting_root({ old_view });
	Ref<RNShadowNode> new_text = mounting_node(2, "RCTText");
	Ref<RNShadowNode> changed_type_root = mounting_root({ new_text });
	Vector<RNMountingMutation> mutations;
	String error;
	CHECK_FALSE(RNTreeDiffer::diff(old_root, changed_type_root, mutations, error));
	CHECK(error.contains("changed host type"));

	Ref<RNShadowNode> foreign = mounting_node(3, "RCTView");
	foreign->surface_epoch = 8;
	Ref<RNShadowNode> foreign_root = mounting_root({ foreign });
	error = String();
	CHECK_FALSE(RNTreeDiffer::diff(Ref<RNShadowNode>(), foreign_root, mutations, error));
	CHECK(error.contains("another surface"));
}

TEST_CASE("[ReactNativeBindings][Mounting] raw text changes update one nearest Text host") {
	Dictionary old_props;
	old_props["text"] = "old";
	Dictionary new_props;
	new_props["text"] = "new";
	Ref<RNShadowNode> old_raw = mounting_node(3, "RCTRawText", old_props);
	Ref<RNShadowNode> new_raw = old_raw->clone(false, &new_props);
	Ref<RNShadowNode> old_text = mounting_node(2, "RCTText");
	old_text->children.push_back(old_raw);
	Ref<RNShadowNode> new_text = clone_with_children(old_text, { new_raw });
	Ref<RNShadowNode> old_root = mounting_root({ old_text });
	Ref<RNShadowNode> new_root = clone_with_children(old_root, { new_text });
	Vector<RNMountingMutation> mutations;
	String error;
	REQUIRE(RNTreeDiffer::diff(old_root, new_root, mutations, error));
	REQUIRE(mutations.size() == 1);
	CHECK(mutations[0].type == RNMutationType::UPDATE);
	CHECK(mutations[0].new_node->tag == 2);
}

TEST_CASE("[ReactNativeBindings][Mounting] inherited pointer context reaches shared descendants") {
	Ref<RNShadowNode> child = mounting_node(3, "RCTView");
	Ref<RNShadowNode> old_parent = mounting_node(2, "RCTView");
	old_parent->children.push_back(child);
	Dictionary disabled_props;
	disabled_props["pointerEvents"] = "none";
	Ref<RNShadowNode> new_parent = old_parent->clone(false, &disabled_props);
	Ref<RNShadowNode> old_root = mounting_root({ old_parent });
	Ref<RNShadowNode> new_root = clone_with_children(old_root, { new_parent });
	Vector<RNMountingMutation> mutations;
	String error;
	REQUIRE(RNTreeDiffer::diff(old_root, new_root, mutations, error));
	REQUIRE(mutations.size() == 2);
	CHECK(mutations[0].type == RNMutationType::UPDATE);
	CHECK(mutations[0].new_node->tag == 2);
	CHECK(mutations[1].type == RNMutationType::UPDATE);
	CHECK(mutations[1].new_node->tag == 3);
}

TEST_CASE("[ReactNativeBindings][Mounting] retained Yoga nodes survive updates and reset removed styles") {
	Dictionary fixed;
	fixed["width"] = 80;
	fixed["height"] = 20;
	Ref<RNShadowNode> view = mounting_node(2, "RCTView", fixed);
	Ref<RNShadowNode> root = mounting_root({ view });
	RNLayoutTree layout;
	HashMap<int, Rect2> rectangles;
	String error;
	REQUIRE(layout.prepare(root, Size2(300, 100), rectangles, error));
	layout.publish();
	CHECK(layout.get_stats().nodes_created == 2);
	const Rect2 *fixed_rect = rectangles.getptr(2);
	REQUIRE(fixed_rect != nullptr);
	CHECK(fixed_rect->size.x == doctest::Approx(80.0));

	layout.reset_stats();
	HashMap<int, Rect2> identical;
	REQUIRE(layout.prepare(root, Size2(300, 100), identical, error));
	CHECK(layout.get_stats().nodes_created == 0);
	CHECK(layout.get_stats().style_writes == 0);
	CHECK(layout.get_stats().calculations == 0);

	Dictionary reset;
	reset["height"] = 20;
	Ref<RNShadowNode> flexible_view = view->clone(false, &reset);
	Ref<RNShadowNode> flexible_root = clone_with_children(root, { flexible_view });
	HashMap<int, Rect2> reset_rectangles;
	REQUIRE(layout.prepare(flexible_root, Size2(300, 100), reset_rectangles, error));
	CHECK(layout.get_stats().nodes_created == 0);
	CHECK(layout.get_stats().style_writes == 1);
	const Rect2 *reset_rect = reset_rectangles.getptr(2);
	REQUIRE(reset_rect != nullptr);
	CHECK(reset_rect->size.x == doctest::Approx(300.0));

	layout.publish();
	layout.reset_stats();
	Ref<RNShadowNode> empty_root = clone_with_children(flexible_root, {});
	HashMap<int, Rect2> removed_rectangles;
	REQUIRE(layout.prepare(empty_root, Size2(300, 100), removed_rectangles, error));
	CHECK(layout.get_stats().nodes_freed == 0);
	layout.publish();
	CHECK(layout.get_stats().nodes_freed == 1);

	layout.reset_stats();
	HashMap<int, Rect2> resized_rectangles;
	REQUIRE(layout.prepare(empty_root, Size2(340, 100), resized_rectangles, error));
	CHECK(layout.get_stats().nodes_created == 0);
	CHECK(layout.get_stats().nodes_freed == 0);
	CHECK(layout.get_stats().calculations == 1);
}

} // namespace TestRNMounting
