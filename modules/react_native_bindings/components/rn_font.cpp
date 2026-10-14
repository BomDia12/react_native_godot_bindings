#include "rn_font.h"

#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "scene/theme/theme_db.h"

#include <algorithm>
#include <cmath>

Ref<Font> rn_load_font_family(const String &p_family, Dictionary &r_variation) {
	const Dictionary aliases = ProjectSettings::get_singleton()->get_setting("react_native/text/font_aliases", Dictionary());
	Variant resource = aliases.get(p_family, p_family);
	if (resource.get_type() == Variant::DICTIONARY) {
		r_variation = resource;
		resource = r_variation.get("font", Variant());
	}
	if (resource.get_type() == Variant::OBJECT) {
		return resource;
	}
	if (resource.get_type() != Variant::STRING) {
		return Ref<Font>();
	}
	const String path = resource;
	if (!path.begins_with("res://") || path.simplify_path() != path) {
		return Ref<Font>();
	}
	Ref<Font> font = ResourceLoader::load(path, "Font");
	if (font.is_null() && (path.get_extension() == "ttf" || path.get_extension() == "otf")) {
		Ref<FontFile> file;
		file.instantiate();
		if (file->load_dynamic_font(path) == OK) {
			font = file;
		}
	}
	return font;
}

Ref<Font> rn_font_variation(const Ref<Font> &p_base, int p_weight, bool p_italic, float p_letter_spacing) {
	if (p_weight == 400 && !p_italic && p_letter_spacing == 0) {
		return p_base;
	}
	Ref<FontVariation> variation;
	variation.instantiate();
	variation->set_base_font(p_base);
	Dictionary coordinates;
	coordinates["wght"] = p_weight;
	variation->set_variation_opentype(coordinates);
	if (p_weight > 400 && !p_base->get_supported_variation_list().has(TS->name_to_tag("wght"))) {
		variation->set_variation_embolden((p_weight - 400) / 250.0f);
	}
	if (p_italic) {
		variation->set_variation_transform(Transform2D(1, 0, -0.2, 1, 0, 0));
	}
	variation->set_spacing(TextServer::SPACING_GLYPH, int(Math::round(p_letter_spacing)));
	return variation;
}

bool rn_resolve_font(const Dictionary &p_props, const RNHostContext &p_context, RNFontSnapshot &r_font, RNError &r_error) {
	r_font.base = p_context.owner ? p_context.owner->get_theme_font("normal_font", "RichTextLabel") : ThemeDB::get_singleton()->get_fallback_font();
	r_font.size = p_context.owner ? p_context.owner->get_theme_font_size("normal_font_size", "RichTextLabel") : ThemeDB::get_singleton()->get_fallback_font_size();
	r_font.color = p_context.owner ? p_context.owner->get_theme_color("default_color", "RichTextLabel") : Color(1, 1, 1);
	r_font.revision = p_context.owner ? p_context.owner->get_native_resource_revision() : 1;
	const String family = p_props.get("fontFamily", String());
	Dictionary variation;
	if (!family.is_empty()) {
		const Ref<Font> base = rn_load_font_family(family, variation);
		if (base.is_null()) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "fontFamily did not resolve to a registered alias or a res:// Godot Font", "font.resolve", "props.fontFamily");
			return false;
		}
		r_font.base = base;
	}
	float size = r_font.size;
	if (p_props.has("fontSize")) {
		if (!RNViewStyle::font_size_of(p_props, size) || !std::isfinite(size) || size <= 0 || size > 4096) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "fontSize must be finite and in (0, 4096]", "font.resolve", "props.fontSize");
			return false;
		}
	}
	const Variant allow = p_props.get("allowFontScaling", true);
	const Variant cap = p_props.get("maxFontSizeMultiplier", Variant());
	if (allow.get_type() != Variant::BOOL || (cap.get_type() != Variant::NIL && ((cap.get_type() != Variant::INT && cap.get_type() != Variant::FLOAT) || !std::isfinite(double(cap)) || double(cap) < 0 || (double(cap) > 0 && double(cap) < 1)))) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid font scaling properties", "font.resolve");
		return false;
	}
	r_font.multiplier = bool(allow) ? p_context.font_scale : 1;
	if (bool(allow) && cap.get_type() != Variant::NIL && double(cap) > 0) {
		r_font.multiplier = std::min(r_font.multiplier, double(cap));
	}
	const double effective = double(size) * r_font.multiplier;
	if (!std::isfinite(effective) || effective <= 0 || effective > 65536) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Scaled font size exceeds native bounds", "font.resolve");
		return false;
	}
	r_font.size = MAX(1, int(Math::round(effective)));
	r_font.revision = hash_murmur3_one_64(p_context.metrics_revision, r_font.revision);
	r_font.revision = hash_murmur3_one_double(r_font.multiplier, r_font.revision);
	if (p_props.has("color") && !RNViewStyle::color_of(p_props, "color", r_font.color)) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid text color", "font.resolve", "props.color");
		return false;
	}
	r_font.font = r_font.base;
	const String weight = p_props.get("fontWeight", variation.get("weight", "normal"));
	const String style = p_props.get("fontStyle", variation.get("style", "normal"));
	const float spacing = p_props.get("letterSpacing", 0.0);
	if (!std::isfinite(spacing) || Math::abs(spacing) > 4096 || (style != "normal" && style != "italic") || (weight != "normal" && weight != "bold" && (!weight.is_valid_int() || weight.to_int() < 100 || weight.to_int() > 900))) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid font weight, style or letterSpacing", "font.resolve");
		return false;
	}
	const int numeric_weight = weight == "bold" ? 700 : weight == "normal" ? 400
																		   : weight.to_int();
	r_font.font = rn_font_variation(r_font.base, numeric_weight, style == "italic", spacing);
	if (r_font.base.is_valid()) {
		r_font.revision = hash_murmur3_one_64(uint64_t(r_font.base->get_instance_id()), r_font.revision);
	}
	return true;
}
