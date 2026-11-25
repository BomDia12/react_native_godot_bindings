#include "rn_image_control.h"

#include "rn_clip.h"
#include "rn_shader_cache.h"

#include "servers/display/accessibility_server.h"

namespace {

constexpr const char *ROUNDED_IMAGE_SHADER = R"(
shader_type canvas_item;
%s
uniform vec4 image_rect;
uniform vec4 image_radii_x;
uniform vec4 image_radii_y;
varying vec2 local_position;
void vertex() {
	local_position = VERTEX;
	%s
}
void fragment() {
	COLOR.a *= clamp(0.5 - rn_rounded_distance(local_position, image_rect, image_radii_x, image_radii_y), 0.0, 1.0) * rn_clip_coverage();
}
)";

} // namespace

void RNImageControl::set_corners(const RNRoundedRect &p_corners) {
	corners = p_corners;
	_update_rounding();
}

// Rounded corners and enclosing rounded clips both need this control's own
// shader; without either, it draws with the default or inherited material.
void RNImageControl::_update_rounding() {
	const Vector<RNClip> stack = is_inside_tree() ? rn_clip_stack(get_parent()) : Vector<RNClip>();
	if (!corners.has_radius() && stack.is_empty()) {
		if (rounding.is_valid()) {
			set_material(Ref<Material>());
			rn_inherit_clip(this);
		}
		set_notify_transform(false);
		return;
	}
	if (rounding.is_null()) {
		rounding.instantiate();
		rounding->set_shader(rn_cached_shader(vformat(ROUNDED_IMAGE_SHADER, rn_clip_shader_code(), RN_CLIP_VERTEX)));
	}
	set_material(rounding);
	set_use_parent_material(false);
	set_notify_transform(true);
	RNRoundedRect shape = corners;
	if (!shape.rect.has_area()) {
		shape.rect = Rect2(Point2(), get_size());
	}
	rounding->set_shader_parameter("image_rect", Vector4(shape.rect.position.x, shape.rect.position.y, shape.rect.size.x, shape.rect.size.y));
	rounding->set_shader_parameter("image_radii_x", Vector4(shape.radii[0].x, shape.radii[1].x, shape.radii[2].x, shape.radii[3].x));
	rounding->set_shader_parameter("image_radii_y", Vector4(shape.radii[0].y, shape.radii[1].y, shape.radii[2].y, shape.radii[3].y));
	rn_set_clip_uniforms(rounding.ptr(), stack);
}

void RNImageControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			const RID element = get_accessibility_element();
			ERR_FAIL_COND(element.is_null());
			AccessibilityServer::get_singleton()->update_set_role(element, AccessibilityServerEnums::ROLE_IMAGE);
		} break;
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case RN_NOTIFICATION_CLIP_CHANGED: {
			_update_rounding();
		} break;
	}
}

RNImageControl::RNImageControl() {
	set_expand_mode(EXPAND_IGNORE_SIZE);
	set_clip_contents(true);
}
void RNImageControl::set_resource(std::shared_ptr<const RNImageResource> p_resource) {
	resource = std::move(p_resource);
	set_texture(resource ? resource->texture : Ref<Texture2D>());
}
