#include "rn_effect_host.h"

#include "rn_clip.h"
#include "rn_shader_cache.h"

#include "core/input/input.h"
#include "core/object/callable_mp.h"
#include "servers/rendering/rendering_server.h"

using namespace facebook::react;

namespace {

// W3C Compositing and Blending Level 1, section 10.
constexpr const char *BLEND_SHADER = R"(
shader_type canvas_item;
%s
uniform sampler2D backdrop : hint_screen_texture, repeat_disable, filter_nearest;
uniform int mode = 0;
varying vec4 rn_modulate;

// Transparent viewports, including the backdrop inside another host, hold
// premultiplied color.
vec4 straight(vec4 c) {
	return c.a > 0.0001 ? vec4(c.rgb / c.a, c.a) : vec4(0.0);
}

float lum(vec3 c) {
	return dot(c, vec3(0.3, 0.59, 0.11));
}

vec3 clip_color(vec3 c) {
	float l = lum(c);
	float n = min(min(c.r, c.g), c.b);
	float x = max(max(c.r, c.g), c.b);
	if (n < 0.0) {
		c = l + (c - l) * l / (l - n);
	}
	if (x > 1.0) {
		c = l + (c - l) * (1.0 - l) / (x - l);
	}
	return c;
}

vec3 set_lum(vec3 c, float l) {
	return clip_color(c + (l - lum(c)));
}

float sat(vec3 c) {
	return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
}

vec3 set_sat(vec3 c, float s) {
	float lo = min(min(c.r, c.g), c.b);
	float hi = max(max(c.r, c.g), c.b);
	return hi > lo ? (c - lo) * s / (hi - lo) : vec3(0.0);
}

vec3 hard_light(vec3 b, vec3 s) {
	return mix(b * 2.0 * s, 1.0 - (1.0 - b) * (1.0 - (2.0 * s - 1.0)), step(0.5, s));
}

vec3 soft_light(vec3 b, vec3 s) {
	vec3 d = mix(sqrt(b), ((16.0 * b - 12.0) * b + 4.0) * b, step(b, vec3(0.25)));
	return mix(b + (2.0 * s - 1.0) * (d - b), b - (1.0 - 2.0 * s) * b * (1.0 - b), step(s, vec3(0.5)));
}

vec3 blend(vec3 b, vec3 s) {
	if (mode == 1) return b * s;
	if (mode == 2) return b + s - b * s;
	if (mode == 3) return hard_light(s, b);
	if (mode == 4) return min(b, s);
	if (mode == 5) return max(b, s);
	if (mode == 6) return mix(min(vec3(1.0), b / max(1.0 - s, vec3(0.00001))), vec3(1.0), step(1.0, s)) * step(0.00001, b);
	if (mode == 7) return mix(1.0 - min(vec3(1.0), (1.0 - b) / max(s, vec3(0.00001))), vec3(0.0), step(s, vec3(0.0))) * (1.0 - step(1.0, b)) + step(1.0, b);
	if (mode == 8) return hard_light(b, s);
	if (mode == 9) return soft_light(b, s);
	if (mode == 10) return abs(b - s);
	if (mode == 11) return b + s - 2.0 * b * s;
	if (mode == 12) return set_lum(set_sat(s, sat(b)), lum(b));
	if (mode == 13) return set_lum(set_sat(b, sat(s)), lum(b));
	if (mode == 14) return set_lum(s, lum(b));
	if (mode == 15) return set_lum(b, lum(s));
	return s;
}

void vertex() {
	rn_modulate = COLOR;
	%s
}

void fragment() {
	vec4 source = straight(texture(TEXTURE, UV)) * rn_modulate;
	vec4 below = straight(texture(backdrop, SCREEN_UV));
	vec3 mixed = (1.0 - below.a) * source.rgb + below.a * clamp(blend(below.rgb, source.rgb), 0.0, 1.0);
	COLOR = vec4(mixed, source.a * rn_clip_coverage());
}
)";

// The viewport texture holds premultiplied color. The inherited modulate, such
// as the view's opacity, applies to the straight color.
constexpr const char *COMPOSITE_SHADER = R"(
shader_type canvas_item;
render_mode %s;
%s
varying vec4 rn_modulate;
void vertex() {
	rn_modulate = COLOR;
	%s
}
void fragment() {
	vec4 texel = texture(TEXTURE, UV);
	COLOR = vec4(texel.a > 0.0001 ? texel.rgb / texel.a : vec3(0.0), texel.a) * rn_modulate;
	COLOR.a *= rn_clip_coverage();
}
)";

} // namespace

RNEffectHost::RNEffectHost() {
	set_stretch(true);
	set_mouse_filter(MOUSE_FILTER_PASS);
	set_focus_mode(FOCUS_ALL);
	set_notify_transform(true);
	viewport = memnew(SubViewport);
	viewport->set_transparent_background(true);
	viewport->set_disable_3d(true);
	viewport->connect("gui_focus_changed", callable_mp(this, &RNEffectHost::_inner_focus_changed));
	add_child(viewport);
	content = memnew(Control);
	content->set_mouse_filter(MOUSE_FILTER_IGNORE);
	viewport->add_child(content);
	painter = memnew(RNViewControl);
	content->add_child(painter);
	filtered_material.instantiate();
	filtered_material->set_shader(rn_cached_shader(vformat(COMPOSITE_SHADER, "blend_mix", rn_clip_shader_code(), RN_CLIP_VERTEX)));
	filter_material.instantiate();
	blend_material.instantiate();
	_update_blend();
	set_material(blend_material);
}

void RNEffectHost::configure(const Size2 &p_size, real_t p_margin, const ::Vector<RNFilterStage> &p_stages, BlendMode p_blend) {
	margin = p_margin;
	set_position(-Vector2(margin, margin));
	set_size(p_size + Vector2(margin, margin) * 2);
	content->set_position(Vector2(margin, margin));
	content->set_size(p_size);
	stages = p_stages;
	_use_filtered_viewport(!stages.is_empty());
	if (filtered) {
		filtered->set_position(-Vector2(margin, margin));
		filtered->set_size(p_size + Vector2(margin, margin) * 2);
		painter->set_position(Vector2(margin, margin));
	}
	painter->set_size(p_size);

	const RID group = content->get_canvas_item();
	if (stages.is_empty()) {
		RS::get_singleton()->canvas_item_set_canvas_group_mode(group, RSE::CANVAS_GROUP_MODE_DISABLED);
		content->set_material(Ref<Material>());
	} else {
		RS::get_singleton()->canvas_item_set_canvas_group_mode(group, RSE::CANVAS_GROUP_MODE_TRANSPARENT, margin, true, margin, rn_filter_blurs(stages));
		const String shader = rn_filter_shader(stages);
		if (shader != filter_shader) {
			filter_shader = shader;
			filter_material->set_shader(rn_cached_shader(shader));
		}
		rn_set_filter_uniforms(filter_material.ptr(), stages);
		content->set_material(filter_material);
	}
	if (blend != p_blend) {
		blend = p_blend;
		_update_blend();
	}
	_update_clip();
}

void RNEffectHost::_use_filtered_viewport(bool p_filtered) {
	if (p_filtered == (filtered != nullptr)) {
		return;
	}
	painter->get_parent()->remove_child(painter);
	if (p_filtered) {
		filtered = memnew(SubViewportContainer);
		filtered->set_stretch(true);
		filtered->set_mouse_filter(MOUSE_FILTER_PASS);
		filtered->set_material(filtered_material);
		filtered_viewport = memnew(SubViewport);
		filtered_viewport->set_transparent_background(true);
		filtered_viewport->set_disable_3d(true);
		filtered_viewport->connect("gui_focus_changed", callable_mp(this, &RNEffectHost::_inner_focus_changed));
		filtered->add_child(filtered_viewport);
		filtered_viewport->add_child(painter);
		content->add_child(filtered);
	} else {
		content->remove_child(filtered);
		memdelete(filtered);
		filtered = nullptr;
		filtered_viewport = nullptr;
		painter->set_position(Point2());
		content->add_child(painter);
	}
}

void RNEffectHost::_update_blend() {
	const String clip = rn_clip_shader_code();
	if (blend == BlendMode::Normal || blend == BlendMode::PlusLighter) {
		blend_material->set_shader(rn_cached_shader(vformat(COMPOSITE_SHADER, blend == BlendMode::Normal ? "blend_mix" : "blend_add", clip, RN_CLIP_VERTEX)));
		RS::get_singleton()->canvas_item_set_copy_to_backbuffer(get_canvas_item(), false, Rect2());
		return;
	}
	blend_material->set_shader(rn_cached_shader(vformat(BLEND_SHADER, clip, RN_CLIP_VERTEX)));
	blend_material->set_shader_parameter("mode", int(blend));
	// Every host blends against what was drawn before it, including earlier hosts.
	// The compatibility renderer copies an item's rectangle without its
	// transform, so the whole viewport is copied.
	RS::get_singleton()->canvas_item_set_copy_to_backbuffer(get_canvas_item(), true, Rect2());
}

void RNEffectHost::_update_clip() {
	rn_set_clip_uniforms(blend_material.ptr(), rn_clip_stack(get_parent()));
}

Control *RNEffectHost::_focusable(bool p_last) const {
	::Vector<Control *> found;
	::Vector<Node *> pending = { painter };
	while (!pending.is_empty()) {
		Node *node = pending[pending.size() - 1];
		pending.resize(pending.size() - 1);
		Control *control = Object::cast_to<Control>(node);
		if (control && control != painter && control->is_visible_in_tree() && control->get_focus_mode_with_override() == FOCUS_ALL) {
			found.push_back(control);
		}
		for (int i = node->get_child_count() - 1; i >= 0; --i) {
			pending.push_back(node->get_child(i));
		}
	}
	if (found.is_empty()) {
		return nullptr;
	}
	return p_last ? found[found.size() - 1] : found[0];
}

// A window has one focused control, so focus moves on to a control inside;
// keys then reach it through the container's unhandled input.
void RNEffectHost::_enter_focus() {
	const bool backwards = Input::get_singleton()->is_action_pressed(SNAME("ui_focus_prev"));
	Control *target = _focusable(backwards);
	if (!target) {
		if (Control *next = backwards ? find_prev_valid_focus() : find_next_valid_focus(); next && next != this) {
			callable_mp(next, &Control::grab_focus).call_deferred(false);
		}
		return;
	}
	_set_focus_paths();
	target->grab_focus();
}

// Points the first and last inner controls at the host's neighbors, so Tab
// leaves the effect viewport instead of wrapping inside it.
void RNEffectHost::_set_focus_paths() {
	_restore_focus_paths();
	Control *first = _focusable(false);
	Control *last = _focusable(true);
	if (!first) {
		return;
	}
	focus_first = first->get_instance_id();
	focus_last = last->get_instance_id();
	saved_previous = first->get_focus_previous();
	saved_next = last->get_focus_next();
	if (Control *previous = find_prev_valid_focus(); previous && previous != this) {
		first->set_focus_previous(first->get_path_to(previous));
	}
	if (Control *next = find_next_valid_focus(); next && next != this) {
		last->set_focus_next(last->get_path_to(next));
	}
}

void RNEffectHost::_restore_focus_paths() {
	if (Control *first = Object::cast_to<Control>(ObjectDB::get_instance(focus_first))) {
		first->set_focus_previous(saved_previous);
	}
	if (Control *last = Object::cast_to<Control>(ObjectDB::get_instance(focus_last))) {
		last->set_focus_next(saved_next);
	}
	focus_first = ObjectID();
	focus_last = ObjectID();
}

// Focus inside takes it from the host, and an unfocused container only
// forwards keys after the window's own navigation has used Tab. So while a
// control inside has focus, keys go to it first, as to a focused container.
void RNEffectHost::_inner_focus_changed(Control *) {
	set_process_input(true);
	set_process_unhandled_input(false);
}

void RNEffectHost::input(const Ref<InputEvent> &p_event) {
	SubViewport *effect_viewport = get_effect_viewport();
	if (!effect_viewport->gui_get_focus_owner()) {
		set_process_input(false);
		set_process_unhandled_input(true);
		return;
	}
	if (Object::cast_to<InputEventMouse>(*p_event) || Object::cast_to<InputEventScreenDrag>(*p_event) || Object::cast_to<InputEventScreenTouch>(*p_event) || Object::cast_to<InputEventGesture>(*p_event)) {
		return;
	}
	// Children and neighbors change while focus is inside, so the paths out
	// are chosen again right before the viewport moves focus.
	if (p_event->is_action_pressed(SNAME("ui_focus_next"), true, true) || p_event->is_action_pressed(SNAME("ui_focus_prev"), true, true)) {
		_set_focus_paths();
	}
	effect_viewport->push_input(p_event);
}

void RNEffectHost::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_FOCUS_ENTER: {
			_enter_focus();
		} break;
		case NOTIFICATION_PREDELETE: {
			_restore_focus_paths();
		} break;
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case RN_NOTIFICATION_CLIP_CHANGED: {
			_update_clip();
		} break;
	}
}
