#include "rn_style_box.h"

#include "../mounting/rn_component_view.h"

#include "core/math/geometry_2d.h"
#include "core/object/class_db.h"
#include "scene/resources/gradient.h"
#include "scene/resources/gradient_texture.h"
#include "servers/rendering/rendering_server.h"

#include <variant>

using namespace facebook::react;

namespace {

constexpr int GRADIENT_RESOLUTION = 256;
constexpr int MAX_TILES = 4096;
// Each corner's arc is split at its middle between the two edges it joins,
// as the diagonal join of CSS borders does.
constexpr Side CORNER_FIRST_EDGE[4] = { SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM };
constexpr Side CORNER_SECOND_EDGE[4] = { SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM, SIDE_LEFT };

::Color transparent(const ::Color &p_color) {
	return ::Color(p_color.r, p_color.g, p_color.b, 0);
}

real_t resolve(const ValueUnit &p_value, real_t p_reference) {
	return p_value.unit == UnitType::Percent ? p_value.value * p_reference / 100 : p_value.value;
}

struct Band {
	RNRoundedRect outer;
	RNRoundedRect inner;
	real_t widths[4] = {};
	::Color colors[4];
	BorderStyle styles[4] = {};
};

// Dashes and dots along one straight edge, between the outer segment a0-a1 and
// the inner segment b0-b1.
void segmented_edge(RNStyleBox::Batch &r_batch, const Point2 &p_a0, const Point2 &p_a1, const Point2 &p_b0, const Point2 &p_b1, real_t p_width, BorderStyle p_style, const ::Color &p_color) {
	const real_t length = p_a0.distance_to(p_a1);
	if (length <= 0 || p_width <= 0) {
		return;
	}
	if (p_style == BorderStyle::Dotted) {
		const int count = MAX(1, int(Math::round((length / p_width + 1) / 2)));
		const real_t radius = p_width / 2;
		for (int i = 0; i < count; ++i) {
			const real_t t = count == 1 ? 0.5f : (radius + i * (length - p_width) / (count - 1)) / length;
			const Point2 center = (p_a0.lerp(p_a1, t) + p_b0.lerp(p_b1, t)) / 2;
			::Vector<Point2> dot;
			for (int step = 0; step < 16; ++step) {
				const real_t angle = Math::TAU * step / 16;
				dot.push_back(center + Vector2(Math::cos(angle), Math::sin(angle)) * radius);
			}
			r_batch.convex(dot, p_color);
		}
		return;
	}
	const real_t dash = p_width * 3;
	const int count = MAX(1, int(Math::round((length / dash + 1) / 2)));
	const real_t step = 1.0f / (2 * count - 1);
	for (int i = 0; i < count; ++i) {
		const real_t from = 2 * i * step;
		const real_t to = from + step;
		r_batch.convex({ p_a0.lerp(p_a1, from), p_a0.lerp(p_a1, to), p_b0.lerp(p_b1, to), p_b0.lerp(p_b1, from) }, p_color);
	}
}

void band(RNStyleBox::Batch &r_batch, const Band &p_band) {
	const ::Vector<Point2> outer = p_band.outer.contour();
	const ::Vector<Point2> inner = p_band.inner.contour();
	const ::Vector<Point2> outer_feather = p_band.outer.grown(0.5).contour();
	const ::Vector<Point2> inner_feather = p_band.inner.grown(-0.5).contour();
	const int per_corner = RNRoundedRect::CORNER_SEGMENTS + 1;
	const int count = outer.size();
	for (int i = 0; i < count; ++i) {
		const int next = (i + 1) % count;
		const int corner = i / per_corner;
		const int step = i % per_corner;
		const bool straight = step == RNRoundedRect::CORNER_SEGMENTS;
		const Side edge = straight ? CORNER_SECOND_EDGE[corner] : (step < RNRoundedRect::CORNER_SEGMENTS / 2 ? CORNER_FIRST_EDGE[corner] : CORNER_SECOND_EDGE[corner]);
		const ::Color color = p_band.colors[edge];
		if (p_band.widths[edge] <= 0 || color.a <= 0) {
			continue;
		}
		if (straight && p_band.styles[edge] != BorderStyle::Solid) {
			segmented_edge(r_batch, outer[i], outer[next], inner[i], inner[next], p_band.widths[edge], p_band.styles[edge], color);
			continue;
		}
		r_batch.convex({ outer[i], outer[next], inner[next], inner[i] }, color);
		const int a = r_batch.add(outer_feather[i], transparent(color));
		const int b = r_batch.add(outer_feather[next], transparent(color));
		const int c = r_batch.add(outer[next], color);
		const int d = r_batch.add(outer[i], color);
		r_batch.triangle(a, b, c);
		r_batch.triangle(a, c, d);
		const int e = r_batch.add(inner[i], color);
		const int f = r_batch.add(inner[next], color);
		const int g = r_batch.add(inner_feather[next], transparent(color));
		const int h = r_batch.add(inner_feather[i], transparent(color));
		r_batch.triangle(e, f, g);
		r_batch.triangle(e, g, h);
	}
}

void fill(RNStyleBox::Batch &r_batch, const RNRoundedRect &p_shape, const ::Color &p_color) {
	if (p_color.a <= 0 || p_shape.rect.size.x <= 0 || p_shape.rect.size.y <= 0) {
		return;
	}
	const ::Vector<Point2> inner = p_shape.grown(-0.5).contour();
	r_batch.convex(inner, p_color);
	r_batch.ring(p_shape.grown(0.5).contour(), transparent(p_color), inner, p_color);
}

Ref<Gradient> gradient_resource(const ::Vector<float> &p_offsets, const ::Vector<::Color> &p_colors) {
	Ref<Gradient> gradient;
	gradient.instantiate();
	gradient->set_offsets(p_offsets);
	gradient->set_colors(p_colors);
	return gradient;
}

// Maps a point inside a tile to the gradient texture's UV.
struct GradientMapping {
	Ref<Texture2D> texture;
	Transform2D tile_to_uv;
};

GradientMapping linear_gradient(const LinearGradient &p_gradient, const Size2 &p_tile) {
	real_t angle = 180;
	if (const Float *degrees = std::get_if<Float>(&p_gradient.direction)) {
		angle = *degrees;
	} else {
		const real_t corner = Math::rad_to_deg(Math::atan2(p_tile.y, p_tile.x));
		switch (std::get<GradientKeyword>(p_gradient.direction)) {
			case GradientKeyword::ToTopRight:
				angle = corner;
				break;
			case GradientKeyword::ToBottomRight:
				angle = 180 - corner;
				break;
			case GradientKeyword::ToBottomLeft:
				angle = 180 + corner;
				break;
			case GradientKeyword::ToTopLeft:
				angle = 360 - corner;
				break;
		}
	}
	const real_t radians = Math::deg_to_rad(angle);
	const Vector2 direction(Math::sin(radians), -Math::cos(radians));
	const real_t length = Math::abs(p_tile.x * direction.x) + Math::abs(p_tile.y * direction.y);
	::Vector<float> offsets;
	::Vector<::Color> colors;
	rn_resolve_color_stops(p_gradient.colorStops, length, offsets, colors);
	Ref<GradientTexture1D> texture;
	texture.instantiate();
	texture->set_width(GRADIENT_RESOLUTION);
	texture->set_gradient(gradient_resource(offsets, colors));
	texture->update_now();
	const Vector2 start = p_tile / 2 - direction * length / 2;
	// u is the distance along the gradient line, as a fraction of its length.
	const Vector2 u_axis = length > 0 ? direction / length : Vector2();
	Transform2D tile_to_uv(Vector2(u_axis.x, 0), Vector2(u_axis.y, 0), Vector2(-u_axis.dot(start), 0.5));
	return { texture, tile_to_uv };
}

GradientMapping radial_gradient(const RadialGradient &p_gradient, const Size2 &p_tile) {
	const RadialGradientPosition &position = p_gradient.position;
	Point2 center = p_tile / 2;
	if (position.left) {
		center.x = resolve(*position.left, p_tile.x);
	} else if (position.right) {
		center.x = p_tile.x - resolve(*position.right, p_tile.x);
	}
	if (position.top) {
		center.y = resolve(*position.top, p_tile.y);
	} else if (position.bottom) {
		center.y = p_tile.y - resolve(*position.bottom, p_tile.y);
	}
	const Vector2 closest_side(MIN(center.x, p_tile.x - center.x), MIN(center.y, p_tile.y - center.y));
	const Vector2 farthest_side(MAX(center.x, p_tile.x - center.x), MAX(center.y, p_tile.y - center.y));
	const bool circle = p_gradient.shape == RadialGradientShape::Circle;
	Vector2 radius;
	if (const auto *dimensions = std::get_if<RadialGradientSize::Dimensions>(&p_gradient.size.value)) {
		radius = Vector2(resolve(dimensions->x, p_tile.x), circle ? resolve(dimensions->x, p_tile.x) : resolve(dimensions->y, p_tile.y));
	} else {
		switch (std::get<RadialGradientSize::SizeKeyword>(p_gradient.size.value)) {
			case RadialGradientSize::SizeKeyword::ClosestSide:
				radius = circle ? Vector2(1, 1) * MIN(closest_side.x, closest_side.y) : closest_side;
				break;
			case RadialGradientSize::SizeKeyword::FarthestSide:
				radius = circle ? Vector2(1, 1) * MAX(farthest_side.x, farthest_side.y) : farthest_side;
				break;
			case RadialGradientSize::SizeKeyword::ClosestCorner:
				radius = circle ? Vector2(1, 1) * closest_side.length() : closest_side * Math::SQRT2;
				break;
			case RadialGradientSize::SizeKeyword::FarthestCorner:
				radius = circle ? Vector2(1, 1) * farthest_side.length() : farthest_side * Math::SQRT2;
				break;
		}
	}
	radius = radius.maxf(0.001f);
	::Vector<float> offsets;
	::Vector<::Color> colors;
	rn_resolve_color_stops(p_gradient.colorStops, radius.x, offsets, colors);
	Ref<GradientTexture2D> texture;
	texture.instantiate();
	texture->set_width(GRADIENT_RESOLUTION);
	texture->set_height(GRADIENT_RESOLUTION);
	texture->set_fill(GradientTexture2D::FILL_RADIAL);
	texture->set_fill_from(Vector2(0.5, 0.5));
	texture->set_fill_to(Vector2(1, 0.5));
	texture->set_gradient(gradient_resource(offsets, colors));
	texture->update_now();
	// The texture's circle of radius 0.5 is the gradient's ending shape.
	const Vector2 scale = Vector2(0.5f, 0.5f) / radius;
	return { texture, Transform2D(Vector2(scale.x, 0), Vector2(0, scale.y), Vector2(0.5f, 0.5f) - center * scale) };
}

struct Axis {
	real_t start = 0;
	real_t size = 0;
	real_t spacing = 0;
	bool repeats = false;
};

// One axis of background tiling: positioning area p_area, tile size p_tile.
Axis tile_axis(real_t p_area_start, real_t p_area, real_t p_tile, real_t p_offset, BackgroundRepeatStyle p_repeat) {
	Axis axis{ p_area_start + p_offset, p_tile, 0, p_repeat != BackgroundRepeatStyle::NoRepeat };
	if (p_repeat == BackgroundRepeatStyle::Round && p_tile > 0) {
		axis.size = p_area / MAX(1.0f, Math::round(p_area / p_tile));
		axis.start = p_area_start;
	} else if (p_repeat == BackgroundRepeatStyle::Space && p_tile > 0) {
		const int count = int(Math::floor(p_area / p_tile));
		if (count >= 2) {
			axis.start = p_area_start;
			axis.spacing = (p_area - count * p_tile) / (count - 1);
		} else {
			axis.repeats = false;
		}
	}
	return axis;
}

void tile_positions(const Axis &p_axis, real_t p_clip_start, real_t p_clip_end, ::Vector<real_t> &r_positions) {
	if (!p_axis.repeats) {
		r_positions.push_back(p_axis.start);
		return;
	}
	const real_t period = p_axis.size + p_axis.spacing;
	if (period <= 0) {
		return;
	}
	const real_t first = p_axis.start + Math::floor((p_clip_start - p_axis.start) / period) * period;
	for (real_t position = first; position < p_clip_end && r_positions.size() < MAX_TILES; position += period) {
		r_positions.push_back(position);
	}
}

void background_image(RNStyleBox::Batch &r_batch, const RNStyleBox::Paint &p_paint, size_t p_index, const ::Vector<Point2> &p_clip) {
	const Rect2 area = p_paint.padding_box().rect;
	Size2 tile = area.size;
	if (!p_paint.sizes.empty()) {
		if (const auto *lengths = std::get_if<BackgroundSizeLengthPercentage>(&p_paint.sizes[p_index % p_paint.sizes.size()])) {
			if (const ValueUnit *x = std::get_if<ValueUnit>(&lengths->x)) {
				tile.x = resolve(*x, area.size.x);
			}
			if (const ValueUnit *y = std::get_if<ValueUnit>(&lengths->y)) {
				tile.y = resolve(*y, area.size.y);
			}
		}
	}
	if (tile.x <= 0 || tile.y <= 0) {
		return;
	}
	Vector2 offset;
	if (!p_paint.positions.empty()) {
		const BackgroundPosition &position = p_paint.positions[p_index % p_paint.positions.size()];
		const Vector2 free_space = area.size - tile;
		if (position.left) {
			offset.x = resolve(*position.left, free_space.x);
		} else if (position.right) {
			offset.x = free_space.x - resolve(*position.right, free_space.x);
		}
		if (position.top) {
			offset.y = resolve(*position.top, free_space.y);
		} else if (position.bottom) {
			offset.y = free_space.y - resolve(*position.bottom, free_space.y);
		}
	}
	const BackgroundRepeat repeat = p_paint.repeats.empty() ? BackgroundRepeat() : p_paint.repeats[p_index % p_paint.repeats.size()];
	const Axis x_axis = tile_axis(area.position.x, area.size.x, tile.x, offset.x, repeat.x);
	const Axis y_axis = tile_axis(area.position.y, area.size.y, tile.y, offset.y, repeat.y);
	tile = Size2(x_axis.size, y_axis.size);

	const BackgroundImage &image = p_paint.images[p_index];
	const GradientMapping mapping = std::holds_alternative<LinearGradient>(image) ? linear_gradient(std::get<LinearGradient>(image), tile) : radial_gradient(std::get<RadialGradient>(image), tile);
	r_batch.texture = mapping.texture;

	const Rect2 clip_bounds = p_paint.border_box.rect;
	::Vector<real_t> xs, ys;
	tile_positions(x_axis, clip_bounds.position.x, clip_bounds.get_end().x, xs);
	tile_positions(y_axis, clip_bounds.position.y, clip_bounds.get_end().y, ys);
	for (real_t y : ys) {
		for (real_t x : xs) {
			const Rect2 rect(x, y, tile.x, tile.y);
			const ::Vector<Point2> tile_polygon = { rect.position, Point2(rect.get_end().x, rect.position.y), rect.get_end(), Point2(rect.position.x, rect.get_end().y) };
			for (const ::Vector<Point2> &polygon : Geometry2D::intersect_polygons(tile_polygon, p_clip)) {
				const int first = r_batch.points.size();
				for (const Point2 &point : polygon) {
					r_batch.add(point, ::Color(1, 1, 1), mapping.tile_to_uv.xform(point - rect.position));
				}
				for (int i = 1; i + 1 < polygon.size(); ++i) {
					r_batch.triangle(first, first + i, first + i + 1);
				}
			}
		}
	}
}

} // namespace

void rn_resolve_color_stops(const std::vector<ColorStop> &p_stops, real_t p_length, ::Vector<float> &r_offsets, ::Vector<::Color> &r_colors) {
	std::vector<std::pair<::Color, std::optional<real_t>>> stops;
	for (const ColorStop &stop : p_stops) {
		if (!stop.color) {
			continue;
		}
		std::optional<real_t> position;
		if (stop.position.unit != UnitType::Undefined) {
			position = p_length > 0 ? resolve(stop.position, p_length) / p_length : 0;
		}
		stops.push_back({ rn_godot_color(stop.color), position });
	}
	if (stops.empty()) {
		return;
	}
	if (!stops.front().second) {
		stops.front().second = 0;
	}
	if (!stops.back().second) {
		stops.back().second = 1;
	}
	real_t previous = *stops.front().second;
	for (auto &stop : stops) {
		if (stop.second) {
			stop.second = MAX(*stop.second, previous);
			previous = *stop.second;
		}
	}
	for (size_t i = 1; i < stops.size(); ++i) {
		if (stops[i].second) {
			continue;
		}
		size_t end = i;
		while (!stops[end].second) {
			++end;
		}
		const real_t from = *stops[i - 1].second;
		const real_t to = *stops[end].second;
		for (size_t j = i; j < end; ++j) {
			stops[j].second = from + (to - from) * (j - i + 1) / (end - i + 1);
		}
	}
	for (const auto &[color, offset] : stops) {
		r_offsets.push_back(CLAMP(*offset, 0.0f, 1.0f));
		r_colors.push_back(color);
	}
}

int RNStyleBox::Batch::add(const Point2 &p_point, const ::Color &p_color, const Point2 &p_uv) {
	points.push_back(p_point);
	colors.push_back(p_color);
	uvs.push_back(p_uv);
	return points.size() - 1;
}

void RNStyleBox::Batch::triangle(int p_a, int p_b, int p_c) {
	indices.push_back(p_a);
	indices.push_back(p_b);
	indices.push_back(p_c);
}

void RNStyleBox::Batch::convex(const ::Vector<Point2> &p_polygon, const ::Color &p_color) {
	const int first = points.size();
	for (const Point2 &point : p_polygon) {
		add(point, p_color);
	}
	for (int i = 1; i + 1 < p_polygon.size(); ++i) {
		triangle(first, first + i, first + i + 1);
	}
}

void RNStyleBox::Batch::ring(const ::Vector<Point2> &p_outer, const ::Color &p_outer_color, const ::Vector<Point2> &p_inner, const ::Color &p_inner_color) {
	const int count = p_outer.size();
	const int first = points.size();
	for (int i = 0; i < count; ++i) {
		add(p_outer[i], p_outer_color);
		add(p_inner[i], p_inner_color);
	}
	for (int i = 0; i < count; ++i) {
		const int outer = first + 2 * i;
		const int next = first + 2 * ((i + 1) % count);
		triangle(outer, next, next + 1);
		triangle(outer, next + 1, outer + 1);
	}
}

real_t RNStyleBox::Paint::extent() const {
	real_t extent = MAX(0, outline_offset + outline_width);
	for (const BoxShadow &shadow : shadows) {
		if (!shadow.inset) {
			extent = MAX(extent, shadow.blurRadius + MAX(0, shadow.spreadDistance) + MAX(Math::abs(shadow.offsetX), Math::abs(shadow.offsetY)));
		}
	}
	return Math::ceil(extent) + 1;
}

void RNStyleBox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bg_color"), &RNStyleBox::get_bg_color);
	ClassDB::bind_method(D_METHOD("get_border_width", "side"), &RNStyleBox::get_border_width);
}

void RNStyleBox::set_paint(const Paint &p_paint) {
	paint = p_paint;
	_build();
	emit_changed();
}

void RNStyleBox::_build() {
	batches.clear();
	Batch shapes;
	fill(shapes, paint.border_box, paint.background);
	batches.push_back(shapes);

	const ::Vector<Point2> clip = paint.border_box.contour();
	for (size_t i = paint.images.size(); i-- > 0;) {
		Batch image;
		background_image(image, paint, i, clip);
		if (!image.indices.is_empty()) {
			batches.push_back(image);
		}
	}

	Batch lines;
	Band border;
	border.outer = paint.border_box;
	border.inner = paint.padding_box();
	for (int side = 0; side < 4; ++side) {
		border.widths[side] = paint.widths[side];
		border.colors[side] = paint.colors[side];
		border.styles[side] = paint.styles[side];
	}
	band(lines, border);
	if (paint.outline_width > 0 && paint.outline_color.a > 0) {
		Band outline;
		outline.inner = paint.border_box.grown(paint.outline_offset);
		outline.outer = outline.inner.grown(paint.outline_width);
		for (int side = 0; side < 4; ++side) {
			outline.widths[side] = paint.outline_width;
			outline.colors[side] = paint.outline_color;
			outline.styles[side] = BorderStyle(paint.outline_style);
		}
		band(lines, outline);
	}
	batches.push_back(lines);
}

void RNStyleBox::draw(RID p_canvas_item, const Rect2 &p_rect) const {
	const bool offset = p_rect.position != Point2();
	for (const Batch &batch : batches) {
		if (batch.indices.is_empty()) {
			continue;
		}
		::Vector<Point2> points = batch.points;
		if (offset) {
			for (Point2 &point : points) {
				point += p_rect.position;
			}
		}
		RS::get_singleton()->canvas_item_add_triangle_array(p_canvas_item, batch.indices, points, batch.colors, batch.uvs, ::Vector<int>(), ::Vector<float>(), batch.texture.is_valid() ? batch.texture->get_rid() : RID());
	}
}

Rect2 RNStyleBox::get_draw_rect(const Rect2 &p_rect) const {
	return p_rect.grow(paint.extent());
}
