#pragma once

#include "core/variant/dictionary.h"
#include "scene/resources/style_box_flat.h"

class RNViewStyle {
public:
	// The transparent default prevents Panel from drawing its theme style.
	static Ref<StyleBoxFlat> build_stylebox(const Dictionary &p_props, bool p_rtl = false);

	static float opacity_of(const Dictionary &p_props);
	static bool clips_contents(const Dictionary &p_props);

	// Reads a validated {$godot: "Color", r, g, b, a} value.
	static bool color_of(const Dictionary &p_props, const String &p_key, Color &r_color);

	static bool font_size_of(const Dictionary &p_props, float &r_size);
};
