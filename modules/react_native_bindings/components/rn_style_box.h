#pragma once

#include "rn_rounded_rect.h"

#include "scene/resources/style_box.h"
#include "scene/resources/texture.h"

#include <react/renderer/components/view/primitives.h>
#include <react/renderer/graphics/BackgroundImage.h>
#include <react/renderer/graphics/BackgroundPosition.h>
#include <react/renderer/graphics/BackgroundRepeat.h>
#include <react/renderer/graphics/BackgroundSize.h>
#include <react/renderer/graphics/BoxShadow.h>

#include <vector>

// Paints a React Native view box in CSS order: background color, background
// images, borders and outline. Box shadows extend past the box and are drawn
// by RNBoxShadows; they still count towards the draw rectangle.
class RNStyleBox : public StyleBox {
	GDCLASS(RNStyleBox, StyleBox);

public:
	struct Paint {
		RNRoundedRect border_box;
		// Indexed by Side: left, top, right, bottom.
		real_t widths[4] = {};
		Color colors[4];
		facebook::react::BorderStyle styles[4] = {};
		Color background = Color(0, 0, 0, 0);
		std::vector<facebook::react::BackgroundImage> images;
		std::vector<facebook::react::BackgroundSize> sizes;
		std::vector<facebook::react::BackgroundPosition> positions;
		std::vector<facebook::react::BackgroundRepeat> repeats;
		std::vector<facebook::react::BoxShadow> shadows;
		Color outline_color = Color(0, 0, 0, 0);
		real_t outline_width = 0;
		real_t outline_offset = 0;
		facebook::react::OutlineStyle outline_style = facebook::react::OutlineStyle::Solid;

		RNRoundedRect padding_box() const { return border_box.inset(widths[SIDE_LEFT], widths[SIDE_TOP], widths[SIDE_RIGHT], widths[SIDE_BOTTOM]); }
		// How far painting reaches outside the border box on each side.
		real_t extent() const;
	};

	struct Batch {
		Vector<int> indices;
		Vector<Point2> points;
		Vector<Color> colors;
		Vector<Point2> uvs;
		Ref<Texture2D> texture;

		int add(const Point2 &p_point, const Color &p_color, const Point2 &p_uv = Point2());
		void triangle(int p_a, int p_b, int p_c);
		void convex(const Vector<Point2> &p_polygon, const Color &p_color);
		// Quads between two contours with the same point count.
		void ring(const Vector<Point2> &p_outer, const Color &p_outer_color, const Vector<Point2> &p_inner, const Color &p_inner_color);
	};

private:
	Paint paint;
	Vector<Batch> batches;
	void _build();

protected:
	static void _bind_methods();

public:
	void set_paint(const Paint &p_paint);
	// Read by scripts, with StyleBoxFlat's names.
	::Color get_bg_color() const { return paint.background; }
	real_t get_border_width(Side p_side) const { return paint.widths[p_side]; }
	const Paint &get_paint() const { return paint; }
	const Vector<Batch> &get_batches() const { return batches; }

	void draw(RID p_canvas_item, const Rect2 &p_rect) const override;
	Rect2 get_draw_rect(const Rect2 &p_rect) const override;
};

// Resolves CSS color stops along a gradient line p_length long into offsets
// between 0 and 1. Transition hints are ignored.
void rn_resolve_color_stops(const std::vector<facebook::react::ColorStop> &p_stops, real_t p_length, Vector<float> &r_offsets, Vector<Color> &r_colors);
