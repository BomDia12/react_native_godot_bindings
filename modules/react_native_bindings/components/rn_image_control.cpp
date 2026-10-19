#include "rn_image_control.h"

#include "rn_clip.h"
#include "rn_shader_cache.h"

#include "core/object/callable_mp.h"

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
RNImageControl::~RNImageControl() {
	_observe_texture(Ref<Texture2D>());
	++source_token;
	if (service && subscription) {
		service->cancel(subscription);
	}
}
void RNImageControl::load(const RNImageSource *p_source, const std::shared_ptr<RNImageService> &p_service) {
	const String key = p_source ? p_source->key : String();
	if (key == source_key && service == p_service) {
		return;
	}
	++source_token;
	if (service && subscription) {
		service->cancel(subscription);
	}
	subscription = 0;
	source_key = key;
	service = p_service;
	if (!p_source) {
		_observe_texture(Ref<Texture2D>());
		resource.reset();
		set_texture(Ref<Texture2D>());
		return;
	}
	if (callbacks.load_start) {
		callbacks.load_start();
	}
	const uint64_t token = source_token;
	const ObjectID host_id = get_instance_id();
	const auto request = service->request(*p_source, [host_id, token, source = *p_source](RNImageResult result) { auto *host = Object::cast_to<RNImageControl>(ObjectDB::get_instance(host_id)); if (host) { host->_loaded(token, source, result); } });
	if (token == source_token) {
		subscription = request;
	}
}
void RNImageControl::_loaded(uint64_t p_token, const RNImageSource &p_source, const RNImageResult &p_result) {
	if (p_token != source_token) {
		return;
	}
	subscription = 0;
	if (p_result.error.is_set()) {
		if (callbacks.error) {
			callbacks.error(p_result.error.message);
		}
	} else {
		resource = p_result.resource;
		set_texture(resource->texture);
		_observe_texture(resource->texture);
		if (callbacks.load) {
			callbacks.load(p_source, Size2(resource->texture->get_size()) / p_source.scale);
		}
	}
	if (callbacks.load_end) {
		callbacks.load_end();
	}
}
void RNImageControl::_observe_texture(const Ref<Texture2D> &p_texture) {
	if (observed_texture == p_texture) {
		return;
	}
	if (observed_texture.is_valid()) {
		observed_texture->disconnect("changed", callable_mp(this, &RNImageControl::_source_changed));
	}
	observed_texture = p_texture;
	if (observed_texture.is_valid()) {
		observed_texture->connect("changed", callable_mp(this, &RNImageControl::_source_changed));
	}
}
void RNImageControl::_source_changed() {
	if (callbacks.source_changed) {
		callbacks.source_changed();
	}
}
