#pragma once

#include "core/variant/dictionary.h"
#include "scene/resources/style_box_flat.h"

// Extracts a React Native View's *paint* properties (background, border, corner radius)
// from its props, kept separate from rn_layout.cpp because painting is not a Yoga
// concern. Reads props["style"] directly, the same way rn_layout.cpp does — RN's
// flattenStyle already collapsed any style array on the JS side before createNode.
class RNViewStyle {
public:
	// Always returns a valid StyleBoxFlat — fully transparent (bg alpha 0, no border)
	// when the style declares no paint properties. A bare Panel would otherwise draw
	// Godot's default theme panel, so callers must set this override unconditionally.
	static Ref<StyleBoxFlat> build_stylebox(const Dictionary &p_props);

	static float opacity_of(const Dictionary &p_props); // style.opacity, default 1.0
	static bool clips_contents(const Dictionary &p_props); // style.overflow === "hidden"

	// Reads a processed-ARGB color key (e.g. "color" on Text). Returns false and leaves
	// r_color untouched when the key is absent or not numeric (warns once on the latter).
	static bool color_of(const Dictionary &p_props, const String &p_key, Color &r_color);

	// Reads style.fontSize. Returns false when absent or not numeric.
	static bool font_size_of(const Dictionary &p_props, float &r_size);
};
