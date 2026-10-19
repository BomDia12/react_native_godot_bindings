#include "rn_clip.h"

#include "../mounting/rn_component_view.h"
#include "rn_shader_cache.h"

#include "scene/main/viewport.h"

namespace {

constexpr const char *CLIP_HOST_SHADER = R"(
shader_type canvas_item;
%s
void vertex() {
	%s
}
void fragment() {
	COLOR.a *= rn_clip_coverage();
}
)";

bool owns_material(const CanvasItem *p_item) {
	return p_item->get_material().is_valid();
}

bool is_rounded_host(const Node *p_node) {
	const RNClipControl *host = Object::cast_to<RNClipControl>(p_node);
	return host && host->get_shape().has_radius();
}

bool encloses_clip(const Node *p_parent) {
	for (const Node *node = p_parent; node && !Object::cast_to<Viewport>(node); node = node->get_parent()) {
		if (is_rounded_host(node)) {
			return true;
		}
		const CanvasItem *item = Object::cast_to<CanvasItem>(node);
		if (item && owns_material(item)) {
			return false;
		}
	}
	return false;
}

void inherit(Node *p_node, bool p_inside) {
	if (Object::cast_to<Viewport>(p_node)) {
		return;
	}
	CanvasItem *item = Object::cast_to<CanvasItem>(p_node);
	bool children_inside = p_inside;
	if (is_rounded_host(p_node)) {
		children_inside = true;
	} else if (item && owns_material(item)) {
		children_inside = false;
	} else if (item) {
		item->set_use_parent_material(p_inside);
	}
	for (int i = 0; i < p_node->get_child_count(true); ++i) {
		inherit(p_node->get_child(i, true), children_inside);
	}
}

} // namespace

String rn_clip_shader_code() {
	return vformat(R"(
uniform int rn_clip_count = 0;
uniform vec4 rn_clip_axes[%d];
uniform vec2 rn_clip_origin[%d];
uniform vec4 rn_clip_rect[%d];
uniform vec4 rn_clip_radii_x[%d];
uniform vec4 rn_clip_radii_y[%d];
varying vec2 rn_canvas_position;

float rn_rounded_distance(vec2 p, vec4 rect, vec4 radii_x, vec4 radii_y) {
	vec2 half_size = rect.zw * 0.5;
	vec2 q = p - (rect.xy + half_size);
	int corner = q.y < 0.0 ? (q.x < 0.0 ? 0 : 1) : (q.x < 0.0 ? 3 : 2);
	vec2 radius = vec2(radii_x[corner], radii_y[corner]);
	vec2 into_corner = abs(q) - (half_size - radius);
	if (into_corner.x > 0.0 && into_corner.y > 0.0 && radius.x > 0.0 && radius.y > 0.0) {
		return (length(into_corner / radius) - 1.0) * min(radius.x, radius.y);
	}
	vec2 edge = abs(q) - half_size;
	return length(max(edge, vec2(0.0))) + min(max(edge.x, edge.y), 0.0);
}

float rn_clip_coverage() {
	float coverage = 1.0;
	for (int i = 0; i < rn_clip_count; i++) {
		vec2 local = vec2(dot(rn_clip_axes[i].xz, rn_canvas_position), dot(rn_clip_axes[i].yw, rn_canvas_position)) + rn_clip_origin[i];
		coverage *= clamp(0.5 - rn_rounded_distance(local, rn_clip_rect[i], rn_clip_radii_x[i], rn_clip_radii_y[i]), 0.0, 1.0);
	}
	return coverage;
}
)",
			RN_MAX_CLIPS, RN_MAX_CLIPS, RN_MAX_CLIPS, RN_MAX_CLIPS, RN_MAX_CLIPS);
}

void rn_set_clip_uniforms(ShaderMaterial *p_material, const Vector<RNClip> &p_stack) {
	PackedVector4Array axes, rects, radii_x, radii_y;
	PackedVector2Array origins;
	for (int i = 0; i < RN_MAX_CLIPS; ++i) {
		const RNClip clip = i < p_stack.size() ? p_stack[i] : RNClip();
		const Transform2D &xform = clip.canvas_to_local;
		const RNRoundedRect &shape = clip.shape;
		axes.push_back(Vector4(xform.columns[0].x, xform.columns[0].y, xform.columns[1].x, xform.columns[1].y));
		origins.push_back(xform.columns[2]);
		rects.push_back(Vector4(shape.rect.position.x, shape.rect.position.y, shape.rect.size.x, shape.rect.size.y));
		radii_x.push_back(Vector4(shape.radii[0].x, shape.radii[1].x, shape.radii[2].x, shape.radii[3].x));
		radii_y.push_back(Vector4(shape.radii[0].y, shape.radii[1].y, shape.radii[2].y, shape.radii[3].y));
	}
	p_material->set_shader_parameter("rn_clip_count", MIN(p_stack.size(), RN_MAX_CLIPS));
	p_material->set_shader_parameter("rn_clip_axes", axes);
	p_material->set_shader_parameter("rn_clip_origin", origins);
	p_material->set_shader_parameter("rn_clip_rect", rects);
	p_material->set_shader_parameter("rn_clip_radii_x", radii_x);
	p_material->set_shader_parameter("rn_clip_radii_y", radii_y);
}

Vector<RNClip> rn_clip_stack(const Node *p_node) {
	Vector<RNClip> stack;
	for (const Node *node = p_node; node && !Object::cast_to<Viewport>(node); node = node->get_parent()) {
		if (const RNClipControl *host = Object::cast_to<RNClipControl>(node); host && host->get_shape().has_radius()) {
			stack.push_back({ host->get_global_transform().affine_inverse(), host->get_shape() });
		}
	}
	return stack;
}

bool rn_clip_contains(const Vector<RNClip> &p_stack, const Point2 &p_canvas_point) {
	for (const RNClip &clip : p_stack) {
		if (clip.shape.signed_distance(clip.canvas_to_local.xform(p_canvas_point)) > 0) {
			return false;
		}
	}
	return true;
}

void rn_inherit_clip(Control *p_subtree) {
	inherit(p_subtree, encloses_clip(p_subtree->get_parent()));
}

RNClipControl::RNClipControl() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	set_focus_mode(FOCUS_NONE);
	set_clip_contents(true);
	set_notify_transform(true);
	clip_material.instantiate();
	clip_material->set_shader(rn_cached_shader(vformat(CLIP_HOST_SHADER, rn_clip_shader_code(), RN_CLIP_VERTEX)));
}

void RNClipControl::set_shape(const RNRoundedRect &p_shape) {
	shape = p_shape;
	// Square clips use Godot's own rectangle clipping, which batches better.
	set_clip_contents(!shape.has_radius());
	set_material(shape.has_radius() ? Ref<Material>(clip_material) : Ref<Material>());
	rn_inherit_clip(this);
	_update_uniforms();
	propagate_notification(RN_NOTIFICATION_CLIP_CHANGED);
}

void RNClipControl::_update_uniforms() {
	const Vector<RNClip> stack = rn_clip_stack(this);
	if (stack.size() > RN_MAX_CLIPS && !reported_depth) {
		reported_depth = true;
		const Node *view = get_parent();
		WARN_PRINT(vformat("E_UNSUPPORTED_STYLE: React tag %d nests more than %d rounded clips; the outermost ones are ignored.", view ? int(view->get_meta(RNComponentView::TAG_META, 0)) : 0, RN_MAX_CLIPS));
	}
	rn_set_clip_uniforms(clip_material.ptr(), stack);
}

void RNClipControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case RN_NOTIFICATION_CLIP_CHANGED: {
			_update_uniforms();
		} break;
	}
}
