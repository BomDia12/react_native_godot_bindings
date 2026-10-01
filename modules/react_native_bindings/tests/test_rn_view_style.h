#pragma once

#include "../fabric/rn_view_style.h"

#include "tests/test_macros.h"

namespace TestRNViewStyle {

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
