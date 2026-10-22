#include "../components/rn_clip.h"
#include "../components/rn_filter.h"
#include "../components/rn_style_box.h"
#include "support/rn_upstream_test_macros.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

namespace TestRNStyle {

using namespace facebook::react;

RNRoundedRect rounded(const Rect2 &p_rect, real_t p_radius) {
	const Vector2 radii[4] = { Vector2(p_radius, p_radius), Vector2(p_radius, p_radius), Vector2(p_radius, p_radius), Vector2(p_radius, p_radius) };
	return RNRoundedRect::normalized(p_rect, radii);
}

ColorStop stop(SharedColor p_color, std::optional<float> p_percent = std::nullopt) {
	ColorStop result;
	result.color = p_color;
	if (p_percent) {
		result.position = ValueUnit(*p_percent, UnitType::Percent);
	}
	return result;
}

// Points of p_color along the inside of the top edge, where only dash ends sit.
int top_edge_breaks(const RNStyleBox::Batch &p_batch, const ::Color &p_color, real_t p_width) {
	int count = 0;
	for (int i = 0; i < p_batch.points.size(); ++i) {
		const Point2 point = p_batch.points[i];
		count += p_batch.colors[i].is_equal_approx(p_color) && point.y <= 2 && point.x > 3 && point.x < p_width - 3 ? 1 : 0;
	}
	return count;
}

int count_color(const RNStyleBox::Batch &p_batch, const ::Color &p_color) {
	int count = 0;
	for (const ::Color &color : p_batch.colors) {
		count += color.is_equal_approx(p_color) ? 1 : 0;
	}
	return count;
}

TEST_CASE("[ReactNativeBindings][Style] rounded rectangles scale overlapping radii and follow CSS spread") {
	const Vector2 radii[4] = { Vector2(80, 80), Vector2(80, 80), Vector2(20, 20), Vector2(20, 20) };
	const RNRoundedRect box = RNRoundedRect::normalized(Rect2(0, 0, 100, 100), radii);
	CHECK(box.radii[0].is_equal_approx(Vector2(50, 50)));
	CHECK(box.radii[2].is_equal_approx(Vector2(12.5, 12.5)));

	const RNRoundedRect square = rounded(Rect2(0, 0, 40, 20), 0);
	CHECK_FALSE(square.has_radius());
	CHECK(square.grown(4).radii[0] == Vector2());
	CHECK(square.grown(4).rect == Rect2(-4, -4, 48, 28));

	const RNRoundedRect circle = rounded(Rect2(0, 0, 40, 40), 20);
	CHECK(circle.grown(2).radii[1].is_equal_approx(Vector2(22, 22)));
	CHECK(circle.inset(4, 2, 0, 0).radii[0].is_equal_approx(Vector2(16, 18)));
	CHECK(circle.contour().size() == 4 * (RNRoundedRect::CORNER_SEGMENTS + 1));
	CHECK(circle.signed_distance(Point2(20, 20)) < 0);
	CHECK(circle.signed_distance(Point2(1, 1)) > 0);
	CHECK(square.signed_distance(Point2(1, 1)) < 0);
}

TEST_CASE("[ReactNativeBindings][Style] color stops fill positions as CSS does") {
	const SharedColor red = colorFromComponents({ .red = 1, .green = 0, .blue = 0, .alpha = 1 });
	const SharedColor green = colorFromComponents({ .red = 0, .green = 1, .blue = 0, .alpha = 1 });
	const SharedColor blue = colorFromComponents({ .red = 0, .green = 0, .blue = 1, .alpha = 1 });
	Vector<float> offsets;
	Vector<::Color> colors;
	rn_resolve_color_stops({ stop(red), stop(green), stop(blue, 80.0f), stop(red, 40.0f) }, 100, offsets, colors);
	REQUIRE(offsets.size() == 4);
	CHECK(offsets[0] == doctest::Approx(0));
	CHECK(offsets[1] == doctest::Approx(0.4));
	CHECK(offsets[2] == doctest::Approx(0.8));
	CHECK(offsets[3] == doctest::Approx(0.8));
	CHECK(colors[1].is_equal_approx(::Color(0, 1, 0)));
}

TEST_CASE("[ReactNativeBindings][Style] borders paint each edge in its own color and split dashes") {
	RNStyleBox::Paint paint;
	paint.border_box = rounded(Rect2(0, 0, 60, 30), 0);
	const ::Color edge_colors[4] = { ::Color(1, 0, 0), ::Color(0, 1, 0), ::Color(0, 0, 1), ::Color(1, 1, 0) };
	for (int side = 0; side < 4; ++side) {
		paint.widths[side] = 2;
		paint.colors[side] = edge_colors[side];
	}
	Ref<RNStyleBox> style;
	style.instantiate();
	style->set_paint(paint);
	const RNStyleBox::Batch &solid = style->get_batches()[style->get_batches().size() - 1];
	for (const ::Color &color : edge_colors) {
		CHECK(count_color(solid, color) > 0);
	}
	CHECK(top_edge_breaks(solid, edge_colors[SIDE_TOP], 60) == 0);

	for (BorderStyle &edge_style : paint.styles) {
		edge_style = BorderStyle::Dashed;
	}
	style->set_paint(paint);
	const RNStyleBox::Batch &dashed = style->get_batches()[style->get_batches().size() - 1];
	// Six dashes along a 60 point edge leave ten dash ends inside it, each on
	// both the outer and inner line.
	CHECK(top_edge_breaks(dashed, edge_colors[SIDE_TOP], 60) == 20);
	CHECK(style->get_draw_rect(Rect2(0, 0, 60, 30)).encloses(Rect2(0, 0, 60, 30)));
}

TEST_CASE("[ReactNativeBindings][Style] gradients tile over the padding box and clip to the border box") {
	RNStyleBox::Paint paint;
	paint.border_box = rounded(Rect2(0, 0, 40, 40), 20);
	LinearGradient gradient;
	gradient.direction = Float(90);
	gradient.colorStops = { stop(colorFromComponents({ .red = 1, .green = 0, .blue = 0, .alpha = 1 })), stop(colorFromComponents({ .red = 0, .green = 0, .blue = 1, .alpha = 1 })) };
	paint.images = { gradient };
	BackgroundSizeLengthPercentage size;
	size.x = ValueUnit(20, UnitType::Point);
	size.y = ValueUnit(20, UnitType::Point);
	paint.sizes = { size };
	Ref<RNStyleBox> style;
	style.instantiate();
	style->set_paint(paint);
	REQUIRE(style->get_batches().size() == 3);
	const RNStyleBox::Batch &tiles = style->get_batches()[1];
	CHECK(tiles.texture.is_valid());
	for (const Point2 &point : tiles.points) {
		CHECK(paint.border_box.signed_distance(point) <= 0.01);
	}
}

TEST_CASE("[ReactNativeBindings][Style] filter color functions fold into one matrix") {
	FilterFunction grayscale{ .type = FilterType::Grayscale, .parameters = Float(1) };
	FilterFunction invert{ .type = FilterType::Invert, .parameters = Float(1) };
	FilterFunction blur{ .type = FilterType::Blur, .parameters = Float(4) };
	const Vector<RNFilterStage> stages = rn_filter_stages({ grayscale, invert, blur, invert });
	REQUIRE(stages.size() == 3);
	CHECK(stages[0].kind == RNFilterStage::COLOR_MATRIX);
	CHECK(stages[1].kind == RNFilterStage::BLUR);
	// Inverted gray: red maps to 1 - 0.2126.
	CHECK(stages[0].matrix[0][0] == doctest::Approx(-0.2126));
	CHECK(stages[0].matrix[0][4] == doctest::Approx(1));
	CHECK(rn_filter_blurs(stages));
	CHECK(rn_filter_extent(stages) == 12);
	CHECK(rn_filter_shader(stages) == rn_filter_shader(rn_filter_stages({ invert, blur, grayscale })));
	CHECK(rn_filter_shader(stages) != rn_filter_shader(rn_filter_stages({ blur })));
}

TEST_CASE("[ReactNativeBindings][Style] stages after a drop shadow filter the shadow too") {
	FilterFunction shadow{ .type = FilterType::DropShadow, .parameters = DropShadowParams{ .offsetX = 2, .offsetY = 2, .standardDeviation = 1, .color = colorFromComponents({ .red = 0, .green = 0, .blue = 0, .alpha = 1 }) } };
	FilterFunction blur{ .type = FilterType::Blur, .parameters = Float(3) };
	const String twice = rn_filter_shader(rn_filter_stages({ shadow, blur }));
	// The shadow is a second layer, and the later blur widens both layers.
	CHECK(twice.contains("vec4 layer_1 = blurred(SCREEN_UV - (vec2(0.0) + stage_shift_0) * SCREEN_PIXEL_SIZE, sqrt(0.0 + stage_radius_0 * stage_radius_0 + stage_radius_1 * stage_radius_1)"));
	CHECK(twice.contains("vec4 layer_0 = blurred(SCREEN_UV - (vec2(0.0)) * SCREEN_PIXEL_SIZE, sqrt(0.0 + stage_radius_1 * stage_radius_1)"));
	// A second drop shadow also shadows the first one.
	CHECK(rn_filter_shader(rn_filter_stages({ shadow, shadow })).contains("layer_3"));
	// Each drop shadow doubles the layers, so only the first few are kept.
	CHECK(rn_filter_stages({ shadow, shadow, shadow, shadow, blur }).size() == RN_MAX_DROP_SHADOWS + 1);
}

TEST_CASE("[ReactNativeBindings][Style][SceneTree] rounded clip hosts stack and square ones clip by rectangle") {
	Control *parent = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(parent);
	RNClipControl *outer = memnew(RNClipControl);
	parent->add_child(outer);
	outer->set_size(Size2(100, 100));
	outer->set_shape(rounded(Rect2(0, 0, 100, 100), 50));
	RNClipControl *inner = memnew(RNClipControl);
	outer->add_child(inner);
	inner->set_position(Point2(50, 0));
	inner->set_size(Size2(50, 50));
	inner->set_shape(rounded(Rect2(0, 0, 50, 50), 0));
	Control *child = memnew(Control);
	inner->add_child(child);
	rn_inherit_clip(child);

	CHECK(outer->get_material().is_valid());
	CHECK_FALSE(outer->is_clipping_contents());
	CHECK(inner->get_material().is_null());
	CHECK(inner->is_clipping_contents());
	CHECK(inner->get_use_parent_material());
	CHECK(child->get_use_parent_material());
	const Vector<RNClip> stack = rn_clip_stack(child);
	REQUIRE(stack.size() == 1);
	CHECK(rn_clip_contains(stack, Point2(50, 50)));
	CHECK_FALSE(rn_clip_contains(stack, Point2(98, 2)));

	inner->remove_child(child);
	parent->add_child(child);
	rn_inherit_clip(child);
	CHECK_FALSE(child->get_use_parent_material());
	memdelete(parent);
}

} // namespace TestRNStyle

void rn_force_link_style_tests() {}
