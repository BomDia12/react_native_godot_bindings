#pragma once

#include "../fabric/rn_event_target.h"
#include "../fabric/rn_shadow_node.h"
#include "../input/rn_input_router.h"

#include "tests/test_macros.h"

namespace TestRNInteraction {

Ref<RNShadowNode> make_node(int p_tag, const String &p_view_name, const Rect2 &p_layout, const Dictionary &p_props = Dictionary()) {
	Ref<RNShadowNode> node;
	node.instantiate();
	node->tag = p_tag;
	node->view_name = p_view_name;
	node->layout = p_layout;
	node->props = p_props;
	return node;
}

Ref<RNShadowNode> make_root(const Vector<Ref<RNShadowNode>> &p_children) {
	Ref<RNShadowNode> root = make_node(1, "RCTRootView", Rect2(Point2(), Size2(300, 300)));
	root->children = p_children;
	return root;
}

TEST_CASE("[ReactNativeBindings][Interaction] clones preserve one weak event target") {
	std::shared_ptr<RNEventTarget> target = std::make_shared<RNEventTarget>(42, 7);
	Ref<RNShadowNode> node = make_node(42, "RCTView", Rect2());
	node->event_target = target;

	for (int children = 0; children < 2; ++children) {
		for (int props = 0; props < 2; ++props) {
			Dictionary replacement;
			Ref<RNShadowNode> clone = node->clone(children != 0, props != 0 ? &replacement : nullptr);
			CHECK(clone->tag == 42);
			CHECK(bool(clone->event_target == target));
			CHECK(clone->event_target->get_generation() == 7);
		}
	}

	CHECK_FALSE(target->has_instance_handle());
	target->reset();
	CHECK_FALSE(target->has_instance_handle());
}

TEST_CASE("[ReactNativeBindings][Interaction] hit testing honors paint order and pointerEvents") {
	Ref<RNShadowNode> back = make_node(2, "RCTView", Rect2(0, 0, 100, 100));
	Ref<RNShadowNode> front = make_node(3, "RCTView", Rect2(20, 20, 100, 100));
	Ref<RNShadowNode> root = make_root({ back, front });
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(30, 30)) == 3);

	front->props["pointerEvents"] = "none";
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(30, 30)) == 2);

	Ref<RNShadowNode> child = make_node(4, "RCTView", Rect2(10, 10, 30, 30));
	front->children = { child };
	front->props["pointerEvents"] = "box-only";
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(35, 35)) == 3);
	front->props["pointerEvents"] = "box-none";
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(35, 35)) == 4);
	front->props["pointerEvents"] = "auto";
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(35, 35)) == 4);
}

TEST_CASE("[ReactNativeBindings][Interaction] hit testing clips children and applies hitSlop") {
	Dictionary parent_props;
	parent_props["overflow"] = "hidden";
	Ref<RNShadowNode> parent = make_node(2, "RCTView", Rect2(20, 20, 40, 40), parent_props);
	Ref<RNShadowNode> child = make_node(3, "RCTView", Rect2(30, 0, 40, 40));
	parent->children.push_back(child);
	Ref<RNShadowNode> root = make_root({ parent });
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(70, 30)) == 0);

	Dictionary slop;
	slop["right"] = 12;
	child->props["hitSlop"] = slop;
	parent->props["overflow"] = "visible";
	CHECK(RNInputRouter::hit_test(root, Size2(300, 300), Point2(101, 30)) == 3);
}

TEST_CASE("[ReactNativeBindings][Interaction] mouse events preserve discrete order and payload fields") {
	Ref<RNShadowNode> target = make_node(2, "RCTView", Rect2(10, 20, 100, 80));
	Ref<RNShadowNode> root = make_root({ target });
	RNRegistry registry;
	RNInputRouter router;

	Ref<InputEventMouseButton> down;
	down.instantiate();
	down->set_button_index(MouseButton::LEFT);
	down->set_pressed(true);
	down->set_ctrl_pressed(true);
	RNInputRouter::RouteResult down_result = router.route_pointer(down, root, registry, Size2(300, 300), 1, 9, Point2(25, 35), Point2(125, 235));
	REQUIRE(down_result.events.size() == 2);
	CHECK(down_result.events[0].name == "topPointerDown");
	CHECK(down_result.events[1].name == "topTouchStart");
	CHECK(int64_t(down_result.events[0].payload["button"]) == 0);
	CHECK(int64_t(down_result.events[0].payload["buttons"]) == 1);
	CHECK(double(down_result.events[0].payload["offsetX"]) == doctest::Approx(15.0));
	CHECK(double(down_result.events[0].payload["offsetY"]) == doctest::Approx(15.0));
	CHECK(bool(down_result.events[0].payload["ctrlKey"]));

	Ref<InputEventMouseButton> up;
	up.instantiate();
	up->set_button_index(MouseButton::LEFT);
	up->set_pressed(false);
	RNInputRouter::RouteResult up_result = router.route_pointer(up, root, registry, Size2(300, 300), 1, 9, Point2(25, 35), Point2(125, 235));
	REQUIRE(up_result.events.size() == 3);
	CHECK(up_result.events[0].name == "topTouchEnd");
	CHECK(up_result.events[1].name == "topPointerUp");
	CHECK(up_result.events[2].name == "topClick");
	CHECK(double(up_result.events[2].payload["pressure"]) == doctest::Approx(0.0));
	CHECK_FALSE(bool(up_result.events[2].payload["isPrimary"]));
}

TEST_CASE("[ReactNativeBindings][Interaction] mouse hover and moves preserve continuous order") {
	Ref<RNShadowNode> target = make_node(2, "RCTView", Rect2(10, 20, 100, 80));
	Ref<RNShadowNode> root = make_root({ target });
	RNRegistry registry;
	RNInputRouter router;

	Ref<InputEventMouseMotion> first;
	first.instantiate();
	first->set_shift_pressed(true);
	RNInputRouter::RouteResult entered = router.route_pointer(first, root, registry, Size2(300, 300), 1, 9, Point2(25, 35), Point2(125, 235));
	REQUIRE(entered.events.size() == 3);
	CHECK(entered.events[0].name == "topPointerOver");
	CHECK(entered.events[1].name == "topPointerEnter");
	CHECK(entered.events[2].name == "topPointerMove");
	for (const RNNativeEvent &event : entered.events) {
		CHECK(event.priority == 2);
		CHECK(event.generation == 9);
	}
	const Dictionary payload = entered.events[2].payload;
	CHECK(double(payload["screenX"]) == doctest::Approx(125.0));
	CHECK(double(payload["clientX"]) == doctest::Approx(25.0));
	CHECK(double(payload["pageX"]) == doctest::Approx(25.0));
	CHECK(double(payload["offsetX"]) == doctest::Approx(15.0));
	CHECK(bool(payload["shiftKey"]));
	CHECK(String(payload["pointerType"]) == "mouse");

	Ref<InputEventMouseMotion> second;
	second.instantiate();
	RNInputRouter::RouteResult moved = router.route_pointer(second, root, registry, Size2(300, 300), 1, 9, Point2(30, 40), Point2(130, 240));
	REQUIRE(moved.events.size() == 1);
	CHECK(moved.events[0].name == "topPointerMove");
	CHECK(double(moved.events[0].payload["clientX"]) == doctest::Approx(30.0));

	RNInputRouter::RouteResult left = router.route_pointer(second, root, registry, Size2(300, 300), 1, 9, Point2(250, 250), Point2(350, 450));
	REQUIRE(left.events.size() == 2);
	CHECK(left.events[0].name == "topPointerOut");
	CHECK(left.events[1].name == "topPointerLeave");
}

TEST_CASE("[ReactNativeBindings][Interaction] secondary mouse buttons use W3C values without clicks") {
	Ref<RNShadowNode> target = make_node(2, "RCTView", Rect2(0, 0, 100, 100));
	Ref<RNShadowNode> root = make_root({ target });
	RNRegistry registry;
	RNInputRouter router;

	auto route_button = [&](MouseButton p_button, bool p_pressed) {
		Ref<InputEventMouseButton> event;
		event.instantiate();
		event->set_button_index(p_button);
		event->set_pressed(p_pressed);
		return router.route_pointer(event, root, registry, Size2(300, 300), 1, 4, Point2(10, 10), Point2(10, 10));
	};

	RNInputRouter::RouteResult right_down = route_button(MouseButton::RIGHT, true);
	REQUIRE(right_down.events.size() == 1);
	CHECK(int64_t(right_down.events[0].payload["button"]) == 2);
	CHECK(int64_t(right_down.events[0].payload["buttons"]) == 2);
	CHECK(right_down.focus_tag == 0);

	RNInputRouter::RouteResult middle_down = route_button(MouseButton::MIDDLE, true);
	REQUIRE(middle_down.events.size() == 1);
	CHECK(int64_t(middle_down.events[0].payload["button"]) == 1);
	CHECK(int64_t(middle_down.events[0].payload["buttons"]) == 6);

	RNInputRouter::RouteResult right_up = route_button(MouseButton::RIGHT, false);
	REQUIRE(right_up.events.size() == 1);
	CHECK(right_up.events[0].name == "topPointerUp");
	CHECK(int64_t(right_up.events[0].payload["buttons"]) == 4);
	CHECK(route_button(MouseButton::MIDDLE, false).events.size() == 1);
}

TEST_CASE("[ReactNativeBindings][Interaction] touch arrays and keyboard activation use the expected contracts") {
	Ref<RNShadowNode> target = make_node(2, "RCTView", Rect2(0, 0, 100, 100));
	Ref<RNShadowNode> root = make_root({ target });
	RNRegistry registry;
	RNInputRouter router;

	Ref<InputEventScreenTouch> press;
	press.instantiate();
	press->set_index(4);
	press->set_pressed(true);
	RNInputRouter::RouteResult touch_result = router.route_pointer(press, root, registry, Size2(300, 300), 1, 2, Point2(12, 13), Point2(22, 23));
	REQUIRE(touch_result.events.size() == 2);
	const Dictionary touch_payload = touch_result.events[1].payload;
	CHECK(Array(touch_payload["changedTouches"]).size() == 1);
	CHECK(Array(touch_payload["touches"]).size() == 1);
	CHECK(int64_t(touch_payload["targetSurface"]) == 1);
	CHECK(bool(touch_result.events[0].payload["isPrimary"]));

	Ref<InputEventKey> enter;
	enter.instantiate();
	enter->set_keycode(Key::ENTER);
	enter->set_pressed(false);
	RNInputRouter::RouteResult key_result = router.route_key(enter, 2, 2);
	REQUIRE(key_result.events.size() == 2);
	CHECK(key_result.events[0].name == "topKeyUp");
	CHECK(String(key_result.events[0].payload["key"]) == "Enter");
	CHECK(String(key_result.events[0].payload["code"]) == "Enter");
	CHECK(key_result.events[1].name == "topClick");
	CHECK_FALSE(key_result.events[1].payload.has("pointerType"));

	enter->set_echo(true);
	CHECK(router.route_key(enter, 2, 2).events.size() == 1);
}

TEST_CASE("[ReactNativeBindings][Interaction] multiple touches track primary contact and cancellation") {
	Ref<RNShadowNode> target = make_node(2, "RCTView", Rect2(0, 0, 100, 100));
	Ref<RNShadowNode> root = make_root({ target });
	RNRegistry registry;
	RNInputRouter router;

	auto route_touch = [&](int p_index, bool p_pressed, bool p_canceled) {
		Ref<InputEventScreenTouch> event;
		event.instantiate();
		event->set_index(p_index);
		event->set_pressed(p_pressed);
		event->set_canceled(p_canceled);
		return router.route_pointer(event, root, registry, Size2(300, 300), 1, 5, Point2(10 + p_index, 20), Point2(10 + p_index, 20));
	};

	RNInputRouter::RouteResult first = route_touch(4, true, false);
	RNInputRouter::RouteResult second = route_touch(9, true, false);
	REQUIRE(first.events.size() == 2);
	REQUIRE(second.events.size() == 2);
	CHECK(bool(first.events[0].payload["isPrimary"]));
	CHECK_FALSE(bool(second.events[0].payload["isPrimary"]));
	CHECK(Array(second.events[1].payload["touches"]).size() == 2);

	RNInputRouter::RouteResult first_end = route_touch(4, false, false);
	REQUIRE(first_end.events.size() == 2);
	CHECK(Array(first_end.events[0].payload["touches"]).size() == 1);
	CHECK(bool(first_end.events[1].payload["isPrimary"]));

	RNInputRouter::RouteResult second_cancel = route_touch(9, false, true);
	REQUIRE(second_cancel.events.size() == 2);
	CHECK(second_cancel.events[0].name == "topTouchCancel");
	CHECK(second_cancel.events[1].name == "topPointerCancel");
	CHECK_FALSE(bool(second_cancel.events[1].payload["isPrimary"]));
	CHECK(Array(second_cancel.events[0].payload["touches"]).is_empty());

	RNInputRouter::RouteResult third = route_touch(7, true, false);
	REQUIRE(third.events.size() == 2);
	CHECK(bool(third.events[0].payload["isPrimary"]));
}

TEST_CASE("[ReactNativeBindings][Interaction] disappearing responders cancel once") {
	Ref<RNShadowNode> target = make_node(2, "RCTView", Rect2(0, 0, 100, 100));
	Ref<RNShadowNode> root = make_root({ target });
	RNRegistry registry;
	RNInputRouter router;

	Ref<InputEventMouseButton> down;
	down.instantiate();
	down->set_button_index(MouseButton::LEFT);
	down->set_pressed(true);
	router.route_pointer(down, root, registry, Size2(300, 300), 1, 8, Point2(10, 10), Point2(10, 10));

	Ref<RNShadowNode> empty_root = make_root({});
	Vector<RNNativeEvent> canceled = router.reconcile_tree(empty_root, registry, 1, 8);
	REQUIRE(canceled.size() == 2);
	CHECK(canceled[0].name == "topTouchCancel");
	CHECK(canceled[1].name == "topPointerCancel");
	CHECK(canceled[0].generation == 8);
	CHECK(router.reconcile_tree(empty_root, registry, 1, 8).is_empty());
}

// JS decides how deep the shadow tree is, and layout, mounting, hit testing and
// measurement all walk it recursively. The limit is what keeps a hostile tree from
// reaching those walks at all.
TEST_CASE("[ReactNativeBindings][Interaction] depth limit accepts real trees and rejects deep ones") {
	Ref<RNShadowNode> shallow_leaf = make_node(3, "RCTView", Rect2());
	Ref<RNShadowNode> shallow_branch = make_node(2, "RCTView", Rect2());
	shallow_branch->children.push_back(shallow_leaf);
	CHECK(RNShadowNode::is_within_depth_limit(make_root({ shallow_branch })));
	CHECK(RNShadowNode::is_within_depth_limit(Ref<RNShadowNode>()));

	Ref<RNShadowNode> deep_root = make_node(1, "RCTRootView", Rect2());
	Ref<RNShadowNode> deepest = deep_root;
	for (int i = 0; i < RNShadowNode::MAX_DEPTH + 1; ++i) {
		Ref<RNShadowNode> child = make_node(i + 2, "RCTView", Rect2());
		deepest->children.push_back(child);
		deepest = child;
	}
	CHECK_FALSE(RNShadowNode::is_within_depth_limit(deep_root));
}

} // namespace TestRNInteraction
