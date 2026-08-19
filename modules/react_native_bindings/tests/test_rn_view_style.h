#pragma once

#include "../fabric/rn_view_style.h"

#include "tests/test_macros.h"

namespace TestRNViewStyle {

TEST_CASE("[ReactNativeBindings][RNViewStyle] decodes processed signed ARGB") {
	Dictionary props;
	props["backgroundColor"] = -13408513.0;
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

} //namespace TestRNViewStyle
