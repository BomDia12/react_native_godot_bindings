#pragma once

#include "core/math/rect2.h"
#include "core/templates/vector.h"

// A CSS box with elliptical corners: top-left, top-right, bottom-right, bottom-left.
// x is the horizontal radius and y the vertical one.
struct RNRoundedRect {
	static constexpr int CORNER_SEGMENTS = 8;

	Rect2 rect;
	Vector2 radii[4];

	// Scales radii down together where adjacent ones overlap a side, as CSS does.
	static RNRoundedRect normalized(const Rect2 &p_rect, const Vector2 p_radii[4]);

	bool has_radius() const;
	// CSS spread: edges move by p_amount and non-zero radii grow with them.
	RNRoundedRect grown(real_t p_amount) const;
	// The inner edge of borders with these widths.
	RNRoundedRect inset(real_t p_left, real_t p_top, real_t p_right, real_t p_bottom) const;
	RNRoundedRect translated(const Vector2 &p_offset) const;
	// Clockwise points, CORNER_SEGMENTS + 1 per corner, starting on the left edge.
	Vector<Point2> contour() const;
	// Negative inside, in pixels.
	real_t signed_distance(const Point2 &p_point) const;
};
