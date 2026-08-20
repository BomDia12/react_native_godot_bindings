#include "rn_view_style.h"

#include "core/error/error_macros.h"
#include "core/math/color.h"

namespace {

Dictionary style_of(const Dictionary &p_props) {
	// Fabric delivers a flat props payload: style keys (backgroundColor, borderWidth, ...)
	// live at the top level, so the props dictionary *is* the style dictionary. There is
	// no nested "style" key on the real RN renderer path.
	return p_props;
}

bool is_number(const Variant &p_value) {
	return p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT;
}

// processColor (JS-side, --platform android) hands us a *signed* 32-bit int in ARGB byte
// order (alpha in the high byte), carried as a JS number -> Variant::FLOAT. The int64_t
// intermediate preserves the sign-extended bit pattern before we reinterpret it unsigned.
Color color_from_packed_argb(double p_value) {
	const uint32_t bits = uint32_t(int64_t(p_value));
	const float a = ((bits >> 24) & 0xFF) / 255.0f;
	const float r = ((bits >> 16) & 0xFF) / 255.0f;
	const float g = ((bits >> 8) & 0xFF) / 255.0f;
	const float b = (bits & 0xFF) / 255.0f;
	return Color(r, g, b, a);
}

// A color key that isn't numeric (e.g. a PlatformColor, which jsi_to_variant turns into a
// Dictionary) is not a crash case: warn once and treat as absent, so the day a non-Android
// bundle or a dynamic color appears it is a loud, localized signal rather than a silent
// garbage box.
bool read_color(const Dictionary &p_style, const String &p_key, Color &r_color) {
	if (!p_style.has(p_key)) {
		return false;
	}
	const Variant value = p_style[p_key];
	if (is_number(value)) {
		r_color = color_from_packed_argb(double(value));
		return true;
	}
	WARN_PRINT_ONCE(vformat("RNViewStyle: non-numeric color for '%s' ignored (expected a processed ARGB int).", p_key).utf8().get_data());
	return false;
}

bool read_number(const Dictionary &p_style, const String &p_key, float &r_value) {
	const Variant value = p_style.get(p_key, Variant());
	if (is_number(value)) {
		r_value = float(value);
		return true;
	}
	return false;
}

} //namespace

Ref<StyleBoxFlat> RNViewStyle::build_stylebox(const Dictionary &p_props) {
	const Dictionary style = style_of(p_props);

	Ref<StyleBoxFlat> box;
	box.instantiate();
	box->set_bg_color(Color(0, 0, 0, 0)); // Transparent default, not Godot's panel gray.

	Color color;
	if (read_color(style, "backgroundColor", color)) {
		box->set_bg_color(color);
	}
	if (read_color(style, "borderColor", color)) {
		box->set_border_color(color);
	}

	// Border width: uniform first, then per-edge overrides. Same four RN keys Yoga
	// reserves box-model space for in rn_layout.cpp, consumed here for painting.
	float width = 0.0f;
	if (read_number(style, "borderWidth", width)) {
		box->set_border_width_all(int(width));
	}
	if (read_number(style, "borderTopWidth", width)) {
		box->set_border_width(SIDE_TOP, int(width));
	}
	if (read_number(style, "borderBottomWidth", width)) {
		box->set_border_width(SIDE_BOTTOM, int(width));
	}
	if (read_number(style, "borderLeftWidth", width)) {
		box->set_border_width(SIDE_LEFT, int(width));
	}
	if (read_number(style, "borderRightWidth", width)) {
		box->set_border_width(SIDE_RIGHT, int(width));
	}

	// Corner radius: uniform first, then per-corner overrides.
	float radius = 0.0f;
	if (read_number(style, "borderRadius", radius)) {
		box->set_corner_radius_all(int(radius));
	}
	if (read_number(style, "borderTopLeftRadius", radius)) {
		box->set_corner_radius(CORNER_TOP_LEFT, int(radius));
	}
	if (read_number(style, "borderTopRightRadius", radius)) {
		box->set_corner_radius(CORNER_TOP_RIGHT, int(radius));
	}
	if (read_number(style, "borderBottomRightRadius", radius)) {
		box->set_corner_radius(CORNER_BOTTOM_RIGHT, int(radius));
	}
	if (read_number(style, "borderBottomLeftRadius", radius)) {
		box->set_corner_radius(CORNER_BOTTOM_LEFT, int(radius));
	}

	return box;
}

float RNViewStyle::opacity_of(const Dictionary &p_props) {
	const Dictionary style = style_of(p_props);
	float opacity = 1.0f;
	// Plain numeric pass-through: opacity is not a color attribute, so do NOT run it
	// through the ARGB unpacker.
	read_number(style, "opacity", opacity);
	return opacity;
}

bool RNViewStyle::clips_contents(const Dictionary &p_props) {
	const Dictionary style = style_of(p_props);
	return String(style.get("overflow", "")) == "hidden";
}

bool RNViewStyle::color_of(const Dictionary &p_props, const String &p_key, Color &r_color) {
	return read_color(style_of(p_props), p_key, r_color);
}

bool RNViewStyle::font_size_of(const Dictionary &p_props, float &r_size) {
	return read_number(style_of(p_props), "fontSize", r_size);
}
