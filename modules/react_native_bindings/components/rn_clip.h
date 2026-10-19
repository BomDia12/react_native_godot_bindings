#pragma once

#include "rn_rounded_rect.h"

#include "scene/gui/control.h"
#include "scene/resources/material.h"

// Rounded overflow clipping without canvas groups, so clips nest freely. A
// clip host's material discards outside its rounded padding box and the boxes
// of the clip hosts above it; descendants inherit it through
// use_parent_material. Controls with a material of their own compose the same
// stack into their shader.

constexpr int RN_MAX_CLIPS = 8;
// Sent down a clip host's subtree when its shape changes, so controls that
// compose its stack refresh their uniforms.
constexpr int RN_NOTIFICATION_CLIP_CHANGED = 8800;

struct RNClip {
	Transform2D canvas_to_local;
	RNRoundedRect shape;
};

class RNClipControl : public Control {
	GDCLASS(RNClipControl, Control);

	RNRoundedRect shape;
	Ref<ShaderMaterial> clip_material;
	bool reported_depth = false;
	void _update_uniforms();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	RNClipControl();
	Size2 get_combined_minimum_size() const override { return Size2(); }
	void set_shape(const RNRoundedRect &p_shape);
	const RNRoundedRect &get_shape() const { return shape; }
};

// Clips that apply to p_node, nearest first, up to the enclosing viewport. A
// clip host includes its own clip.
Vector<RNClip> rn_clip_stack(const Node *p_node);
bool rn_clip_contains(const Vector<RNClip> &p_stack, const Point2 &p_canvas_point);

// Shader source shared by every material that composes a clip stack. Call
// RN_CLIP_VERTEX in vertex() and multiply by rn_clip_coverage() in fragment().
String rn_clip_shader_code();
constexpr const char *RN_CLIP_VERTEX = "rn_canvas_position = (MODEL_MATRIX * vec4(VERTEX, 0.0, 1.0)).xy;";
void rn_set_clip_uniforms(ShaderMaterial *p_material, const Vector<RNClip> &p_stack);

// Points the canvas items of p_subtree at the nearest clip host's material
// when one encloses them, or back at their own.
void rn_inherit_clip(Control *p_subtree);
