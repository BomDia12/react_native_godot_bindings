#pragma once

#include "../components/rn_small_controls.h"
#include "../components/rn_view_control.h"
#include "../components/rn_visual_style.h"
#include "../fabric/rn_view_style.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

namespace TestRNViewStyle {

TEST_CASE("[ReactNativeBindings][VisualStyle] transform lists compose in RN order around the origin") {
	Dictionary props;
	Array operations;
	Dictionary translation;
	translation["translateX"] = "50%";
	Dictionary scale;
	scale["scale"] = 2;
	operations.push_back(translation);
	operations.push_back(scale);
	props["transform"] = operations;
	RNError error;
	REQUIRE(RNVisualStyle::validate(props, error));
	const Transform2D matrix = RNVisualStyle::transform_matrix(props, Size2(100, 40));
	CHECK(matrix.get_origin().is_equal_approx(Point2(50, 0)));
	CHECK(matrix.get_scale().is_equal_approx(Size2(2, 2)));
	CHECK(RNVisualStyle::transform_origin(props, Size2(100, 40)).is_equal_approx(Point2(50, 20)));
	Array origin;
	origin.push_back("25%");
	origin.push_back(10);
	props["transformOrigin"] = origin;
	CHECK(RNVisualStyle::transform_origin(props, Size2(100, 40)).is_equal_approx(Point2(25, 10)));

	Dictionary perspective;
	perspective["perspective"] = 300;
	operations.push_back(perspective);
	CHECK_FALSE(RNVisualStyle::validate(props, error));
	operations.pop_back();
	translation["translateX"] = "nan%";
	CHECK_FALSE(RNVisualStyle::validate(props, error));
}

TEST_CASE("[ReactNativeBindings][VisualStyle][SceneTree] transforms use the stock offset transform around the origin") {
	RNViewControl *control = memnew(RNViewControl);
	SceneTree::get_singleton()->get_root()->add_child(control);
	control->set_position(Point2(10, 20));
	control->set_size(Size2(100, 40));
	Dictionary props;
	Array operations;
	Dictionary translation;
	translation["translateX"] = 5;
	Dictionary rotation;
	rotation["rotate"] = "90deg";
	Dictionary scale;
	scale["scale"] = 2;
	operations.push_back(translation);
	operations.push_back(rotation);
	operations.push_back(scale);
	props["transform"] = operations;
	RNVisualStyle::apply(control, props);
	CHECK(control->is_offset_transform_enabled());
	CHECK_FALSE(control->is_offset_transform_visual_only());
	CHECK(control->get_position() == Point2(10, 20));
	const Transform2D expected = Transform2D(0, Point2(10, 20)) * Transform2D(0, Point2(50, 20)) * RNVisualStyle::transform_matrix(props, control->get_size()) * Transform2D(0, Point2(-50, -20));
	const Transform2D actual = control->get_transform();
	for (const Point2 &point : { Point2(), Point2(100, 0), Point2(30, 40) }) {
		CHECK(actual.xform(point).is_equal_approx(expected.xform(point)));
	}
	RNVisualStyle::apply(control, Dictionary());
	CHECK_FALSE(control->is_offset_transform_enabled());
	memdelete(control);
}

TEST_CASE("[ReactNativeBindings][VisualStyle][SceneTree] skew applies its rotation and scale and reports once per host") {
	RNViewControl *control = memnew(RNViewControl);
	control->set_size(Size2(100, 40));
	Dictionary props;
	Array operations;
	Dictionary skew;
	skew["skewX"] = "30deg";
	operations.push_back(skew);
	props["transform"] = operations;
	RNError error;
	REQUIRE(RNVisualStyle::validate(props, error));
	ERR_PRINT_OFF;
	RNVisualStyle::apply(control, props);
	ERR_PRINT_ON;
	CHECK(control->is_offset_transform_enabled());
	CHECK(control->has_meta("_react_native_skew_reported"));
	CHECK(control->get_offset_transform().get_skew() == doctest::Approx(0.0));
	memdelete(control);
}

TEST_CASE("[ReactNativeBindings][VisualStyle][SceneTree] host controls accept Yoga sizes below their native minimum") {
	RNViewControl *control = memnew(RNViewControl);
	control->set_custom_minimum_size(Size2(100, 80));
	control->set_size(Size2(5, 4));
	CHECK(control->get_size() == Size2(5, 4));
	memdelete(control);
	RNSwitchControl *toggle = memnew(RNSwitchControl);
	toggle->set_size(Size2(2, 2));
	CHECK(toggle->get_size() == Size2(2, 2));
	memdelete(toggle);
}

TEST_CASE("[ReactNativeBindings][RNViewStyle] reads explicit RGBA channels") {
	Dictionary props;
	Dictionary rgba;
	rgba["$godot"] = "Color";
	rgba["r"] = 0.2;
	rgba["g"] = 0.4;
	rgba["b"] = 1.0;
	rgba["a"] = 1.0;
	props["backgroundColor"] = rgba;
	Color color;

	CHECK(RNViewStyle::color_of(props, "backgroundColor", color));
	CHECK(color.is_equal_approx(Color(0.2, 0.4, 1.0, 1.0)));
}

TEST_CASE("[ReactNativeBindings][RNViewStyle] uses transparent defaults") {
	CHECK(RNViewStyle::opacity_of(Dictionary()) == doctest::Approx(1.0));
	CHECK_FALSE(RNViewStyle::clips_contents(Dictionary()));

	Ref<StyleBoxFlat> box = RNViewStyle::build_stylebox(Dictionary());
	REQUIRE(box.is_valid());
	CHECK(box->get_bg_color().a == doctest::Approx(0.0));
}

TEST_CASE("[ReactNativeBindings][RNViewStyle] rejects unsupported color values") {
	Dictionary props;
	props["backgroundColor"] = Dictionary();
	Color color;

	ERR_PRINT_OFF;
	const bool found = RNViewStyle::color_of(props, "backgroundColor", color);
	ERR_PRINT_ON;
	CHECK_FALSE(found);
}

TEST_CASE("[ReactNativeBindings][RNViewStyle] rejects packed and malformed colors") {
	Color color;

	for (const double value : { 1e30, -1e30, double(NAN), double(INFINITY) }) {
		Dictionary props;
		props["backgroundColor"] = value;

		ERR_PRINT_OFF;
		const bool found = RNViewStyle::color_of(props, "backgroundColor", color);
		ERR_PRINT_ON;
		CHECK_FALSE(found);
	}

	Dictionary malformed;
	malformed["$godot"] = "Color";
	malformed["r"] = 2.0;
	malformed["g"] = 0.0;
	malformed["b"] = 0.0;
	malformed["a"] = 1.0;
	Dictionary props;
	props["backgroundColor"] = malformed;
	ERR_PRINT_OFF;
	CHECK_FALSE(RNViewStyle::color_of(props, "backgroundColor", color));
	ERR_PRINT_ON;
}

} //namespace TestRNViewStyle
