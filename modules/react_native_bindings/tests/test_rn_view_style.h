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

// A number outside the 32-bit range has no defined conversion to the packed ARGB integer,
// so it must be refused rather than reinterpreted.
TEST_CASE("[ReactNativeBindings][RNViewStyle] rejects color numbers outside the 32-bit range") {
	Color color;

	for (const double value : { 1e30, -1e30, double(NAN), double(INFINITY) }) {
		Dictionary props;
		props["backgroundColor"] = value;

		ERR_PRINT_OFF;
		const bool found = RNViewStyle::color_of(props, "backgroundColor", color);
		ERR_PRINT_ON;
		CHECK_FALSE(found);
	}

	Dictionary unsigned_props;
	unsigned_props["backgroundColor"] = 4294967295.0;
	CHECK(RNViewStyle::color_of(unsigned_props, "backgroundColor", color));
	CHECK(color.is_equal_approx(Color(1.0, 1.0, 1.0, 1.0)));
}

} //namespace TestRNViewStyle
