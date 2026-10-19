#include "rn_box_shadows.h"

#include "../mounting/rn_component_view.h"
#include "rn_shader_cache.h"

#include "servers/rendering/rendering_server.h"

using namespace facebook::react;

namespace {

// Evan Wallace's closed form for a Gaussian-blurred rounded box, integrated
// along y in four steps.
constexpr const char *SHADOW_SHADER = R"(
shader_type canvas_item;
%s
uniform bool inset = false;
uniform int shadow_count = 0;
uniform vec4 shadow_rect[%d];
uniform vec4 shadow_radii[%d];
uniform vec4 shadow_color[%d];
uniform float shadow_sigma[%d];
uniform vec4 box_rect;
uniform vec4 box_radii_x;
uniform vec4 box_radii_y;
varying vec2 local_position;

vec2 erf2(vec2 x) {
	vec2 s = sign(x);
	vec2 a = abs(x);
	x = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
	x *= x;
	return s - s / (x * x);
}

float gaussian(float x, float sigma) {
	return exp(-(x * x) / (2.0 * sigma * sigma)) / (2.5066283 * sigma);
}

float blurred_row(float x, float y, float sigma, float corner, vec2 half_size) {
	float delta = min(half_size.y - corner - abs(y), 0.0);
	float curved = half_size.x - corner + sqrt(max(0.0, corner * corner - delta * delta));
	vec2 integral = 0.5 + 0.5 * erf2((x + vec2(-curved, curved)) * (0.70710678 / sigma));
	return integral.y - integral.x;
}

float shadow_alpha(vec2 p, vec4 rect, vec4 radii, float sigma) {
	vec2 half_size = rect.zw * 0.5;
	vec2 q = p - (rect.xy + half_size);
	float corner = q.y < 0.0 ? (q.x < 0.0 ? radii.x : radii.y) : (q.x < 0.0 ? radii.w : radii.z);
	corner = min(corner, min(half_size.x, half_size.y));
	if (sigma < 0.01) {
		vec2 into_corner = abs(q) - (half_size - corner);
		float distance = length(max(into_corner, vec2(0.0))) + min(max(into_corner.x, into_corner.y), 0.0) - corner;
		return clamp(0.5 - distance, 0.0, 1.0);
	}
	float low = q.y - half_size.y;
	float high = q.y + half_size.y;
	float start = clamp(-3.0 * sigma, low, high);
	float end = clamp(3.0 * sigma, low, high);
	float step = (end - start) / 4.0;
	float y = start + step * 0.5;
	float value = 0.0;
	for (int i = 0; i < 4; i++) {
		value += blurred_row(q.x, q.y - y, sigma, corner, half_size) * gaussian(y, sigma) * step;
		y += step;
	}
	return value;
}

void vertex() {
	local_position = VERTEX;
	%s
}

void fragment() {
	float box = clamp(0.5 - rn_rounded_distance(local_position, box_rect, box_radii_x, box_radii_y), 0.0, 1.0);
	vec4 result = vec4(0.0);
	for (int i = shadow_count - 1; i >= 0; i--) {
		float alpha = shadow_alpha(local_position, shadow_rect[i], shadow_radii[i], shadow_sigma[i]);
		if (inset) {
			alpha = 1.0 - alpha;
		}
		float a = shadow_color[i].a * alpha;
		result = vec4(shadow_color[i].rgb * a + result.rgb * (1.0 - a), a + result.a * (1.0 - a));
	}
	float mask = inset ? box : 1.0 - box;
	COLOR = vec4(result.a > 0.0 ? result.rgb / result.a : vec3(0.0), result.a * mask * rn_clip_coverage());
}
)";

Vector4 corner_radii(const RNRoundedRect &p_shape) {
	return Vector4((p_shape.radii[0].x + p_shape.radii[0].y) / 2, (p_shape.radii[1].x + p_shape.radii[1].y) / 2, (p_shape.radii[2].x + p_shape.radii[2].y) / 2, (p_shape.radii[3].x + p_shape.radii[3].y) / 2);
}

} // namespace

RNBoxShadows::~RNBoxShadows() {
	_free(outset);
	_free(inset);
}

void RNBoxShadows::_free(Layer &r_layer) {
	if (r_layer.item.is_valid()) {
		RS::get_singleton()->free_rid(r_layer.item);
		r_layer.item = RID();
		r_layer.material.unref();
	}
}

void RNBoxShadows::update(RID p_parent, const RNStyleBox::Paint &p_paint) {
	_update(outset, p_parent, false, p_paint);
	_update(inset, p_parent, true, p_paint);
}

void RNBoxShadows::_update(Layer &r_layer, RID p_parent, bool p_inset, const RNStyleBox::Paint &p_paint) {
	PackedVector4Array rects, radii, colors;
	PackedFloat32Array sigmas;
	const RNRoundedRect box = p_inset ? p_paint.padding_box() : p_paint.border_box;
	for (const BoxShadow &shadow : p_paint.shadows) {
		const ::Color color = rn_godot_color(shadow.color, ::Color(0, 0, 0));
		if (shadow.inset != p_inset || color.a <= 0 || rects.size() == MAX_SHADOWS) {
			continue;
		}
		const RNRoundedRect shape = box.grown(p_inset ? -shadow.spreadDistance : shadow.spreadDistance).translated(Vector2(shadow.offsetX, shadow.offsetY));
		rects.push_back(Vector4(shape.rect.position.x, shape.rect.position.y, shape.rect.size.x, shape.rect.size.y));
		radii.push_back(corner_radii(shape));
		colors.push_back(Vector4(color.r, color.g, color.b, color.a));
		sigmas.push_back(MAX(0, shadow.blurRadius) / 2);
	}
	if (rects.is_empty()) {
		_free(r_layer);
		return;
	}
	if (r_layer.item.is_null()) {
		r_layer.item = RS::get_singleton()->canvas_item_create();
		r_layer.material.instantiate();
		r_layer.material->set_shader(rn_cached_shader(vformat(SHADOW_SHADER, rn_clip_shader_code(), MAX_SHADOWS, MAX_SHADOWS, MAX_SHADOWS, MAX_SHADOWS, RN_CLIP_VERTEX)));
		r_layer.material->set_shader_parameter("inset", p_inset);
		RS::get_singleton()->canvas_item_set_material(r_layer.item, r_layer.material->get_rid());
		if (p_inset) {
			RS::get_singleton()->canvas_item_set_draw_index(r_layer.item, -1);
		} else {
			RS::get_singleton()->canvas_item_set_draw_behind_parent(r_layer.item, true);
		}
	}
	RS::get_singleton()->canvas_item_set_parent(r_layer.item, p_parent);
	const int count = rects.size();
	for (int i = count; i < MAX_SHADOWS; ++i) {
		rects.push_back(Vector4());
		radii.push_back(Vector4());
		colors.push_back(Vector4());
		sigmas.push_back(0);
	}
	r_layer.material->set_shader_parameter("shadow_count", count);
	r_layer.material->set_shader_parameter("shadow_rect", rects);
	r_layer.material->set_shader_parameter("shadow_radii", radii);
	r_layer.material->set_shader_parameter("shadow_color", colors);
	r_layer.material->set_shader_parameter("shadow_sigma", sigmas);
	r_layer.material->set_shader_parameter("box_rect", Vector4(box.rect.position.x, box.rect.position.y, box.rect.size.x, box.rect.size.y));
	r_layer.material->set_shader_parameter("box_radii_x", Vector4(box.radii[0].x, box.radii[1].x, box.radii[2].x, box.radii[3].x));
	r_layer.material->set_shader_parameter("box_radii_y", Vector4(box.radii[0].y, box.radii[1].y, box.radii[2].y, box.radii[3].y));
	RS::get_singleton()->canvas_item_clear(r_layer.item);
	const Rect2 area = p_inset ? box.rect : p_paint.border_box.rect.grow(p_paint.extent());
	RS::get_singleton()->canvas_item_add_rect(r_layer.item, area, ::Color(1, 1, 1));
}

void RNBoxShadows::update_clip(const ::Vector<RNClip> &p_stack) {
	for (Layer *layer : { &outset, &inset }) {
		if (layer->material.is_valid()) {
			rn_set_clip_uniforms(layer->material.ptr(), p_stack);
		}
	}
}
