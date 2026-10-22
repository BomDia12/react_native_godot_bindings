#include "rn_rounded_rect.h"

#include "core/math/math_funcs.h"

RNRoundedRect RNRoundedRect::normalized(const Rect2 &p_rect, const Vector2 p_radii[4]) {
	RNRoundedRect result;
	result.rect = p_rect;
	for (int i = 0; i < 4; ++i) {
		result.radii[i] = p_radii[i];
	}
	const real_t sums[4] = { result.radii[0].x + result.radii[1].x, result.radii[1].y + result.radii[2].y, result.radii[2].x + result.radii[3].x, result.radii[3].y + result.radii[0].y };
	const real_t sides[4] = { p_rect.size.x, p_rect.size.y, p_rect.size.x, p_rect.size.y };
	real_t scale = 1;
	for (int i = 0; i < 4; ++i) {
		if (sums[i] > sides[i]) {
			scale = MIN(scale, sides[i] / sums[i]);
		}
	}
	for (Vector2 &radius : result.radii) {
		radius = (radius * scale).maxf(0);
	}
	return result;
}

bool RNRoundedRect::has_radius() const {
	for (const Vector2 &radius : radii) {
		if (radius.x > 0 && radius.y > 0) {
			return true;
		}
	}
	return false;
}

RNRoundedRect RNRoundedRect::grown(real_t p_amount) const {
	RNRoundedRect result = *this;
	result.rect = rect.grow(p_amount);
	result.rect.size = result.rect.size.maxf(0);
	for (Vector2 &radius : result.radii) {
		if (radius.x > 0 && radius.y > 0) {
			radius = (radius + Vector2(p_amount, p_amount)).maxf(0);
		}
	}
	return result;
}

RNRoundedRect RNRoundedRect::inset(real_t p_left, real_t p_top, real_t p_right, real_t p_bottom) const {
	RNRoundedRect result = *this;
	result.rect.position += Vector2(p_left, p_top);
	result.rect.size = (rect.size - Vector2(p_left + p_right, p_top + p_bottom)).maxf(0);
	result.radii[0] = (radii[0] - Vector2(p_left, p_top)).maxf(0);
	result.radii[1] = (radii[1] - Vector2(p_right, p_top)).maxf(0);
	result.radii[2] = (radii[2] - Vector2(p_right, p_bottom)).maxf(0);
	result.radii[3] = (radii[3] - Vector2(p_left, p_bottom)).maxf(0);
	return result;
}

RNRoundedRect RNRoundedRect::translated(const Vector2 &p_offset) const {
	RNRoundedRect result = *this;
	result.rect.position += p_offset;
	return result;
}

Vector<Point2> RNRoundedRect::contour() const {
	const Point2 centers[4] = {
		rect.position + radii[0],
		Point2(rect.get_end().x - radii[1].x, rect.position.y + radii[1].y),
		rect.get_end() - radii[2],
		Point2(rect.position.x + radii[3].x, rect.get_end().y - radii[3].y),
	};
	const real_t start_angles[4] = { Math::PI, Math::PI * 1.5f, 0, Math::PI * 0.5f };
	Vector<Point2> points;
	points.resize(4 * (CORNER_SEGMENTS + 1));
	Point2 *write = points.ptrw();
	for (int corner = 0; corner < 4; ++corner) {
		for (int step = 0; step <= CORNER_SEGMENTS; ++step) {
			const real_t angle = start_angles[corner] + Math::PI * 0.5f * step / CORNER_SEGMENTS;
			*write++ = centers[corner] + Vector2(radii[corner].x * Math::cos(angle), radii[corner].y * Math::sin(angle));
		}
	}
	return points;
}

real_t RNRoundedRect::signed_distance(const Point2 &p_point) const {
	const Vector2 half = rect.size * 0.5f;
	const Vector2 q = p_point - rect.get_center();
	const int corner = q.y < 0 ? (q.x < 0 ? 0 : 1) : (q.x < 0 ? 3 : 2);
	const Vector2 radius = radii[corner];
	const Vector2 into_corner = q.abs() - (half - radius);
	if (into_corner.x > 0 && into_corner.y > 0 && radius.x > 0 && radius.y > 0) {
		return ((into_corner / radius).length() - 1) * MIN(radius.x, radius.y);
	}
	const Vector2 edge = q.abs() - half;
	return edge.maxf(0).length() + MIN(MAX(edge.x, edge.y), 0);
}
