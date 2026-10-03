#include "rn_font.h"

#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "scene/theme/theme_db.h"

#include <cmath>

bool rn_resolve_font(const Dictionary &p_props, const RNHostContext &p_context, RNFontSnapshot &r_font, RNError &r_error) {
	r_font.base = p_context.owner ? p_context.owner->get_theme_font("normal_font", "RichTextLabel") : ThemeDB::get_singleton()->get_fallback_font();
	r_font.size = p_context.owner ? p_context.owner->get_theme_font_size("normal_font_size", "RichTextLabel") : ThemeDB::get_singleton()->get_fallback_font_size();
	r_font.color = p_context.owner ? p_context.owner->get_theme_color("default_color", "RichTextLabel") : Color(1, 1, 1);
	r_font.revision = p_context.owner ? p_context.owner->get_native_resource_revision() : 1;
	const String family = p_props.get("fontFamily", String());
	Dictionary variation;
	if (!family.is_empty()) {
		const Dictionary aliases = ProjectSettings::get_singleton()->get_setting("react_native/text/font_aliases", Dictionary());
		Variant resource = aliases.get(family, family);
		if (resource.get_type() == Variant::DICTIONARY) {
			variation = resource;
			resource = variation.get("font", Variant());
		}
		if (resource.get_type() == Variant::OBJECT) {
			r_font.base = resource;
		} else if (resource.get_type() == Variant::STRING) {
			const String path = resource;
			if (!path.begins_with("res://") || path.simplify_path() != path) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "fontFamily must name a registered alias or a res:// font resource", "font.resolve", "props.fontFamily");
				return false;
			}
			r_font.base = ResourceLoader::load(path, "Font");
			if (r_font.base.is_null() && (path.get_extension() == "ttf" || path.get_extension() == "otf")) {
				Ref<FontFile> file;
				file.instantiate();
				if (file->load_dynamic_font(path) == OK) {
					r_font.base = file;
				}
			}
		}
		if (r_font.base.is_null()) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "fontFamily did not resolve to a Godot Font", "font.resolve", "props.fontFamily");
			return false;
		}
	}
	float size = r_font.size;
	if (p_props.has("fontSize")) {
		if (!RNViewStyle::font_size_of(p_props, size) || !std::isfinite(size) || size <= 0 || size > 4096) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "fontSize must be finite and in (0, 4096]", "font.resolve", "props.fontSize");
			return false;
		}
		r_font.size = MAX(1, int(Math::round(size)));
	}
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
	if (weight != "normal" || style != "normal" || spacing != 0) {
		Ref<FontVariation> isolated;
		isolated.instantiate();
		isolated->set_base_font(r_font.base);
		Dictionary coordinates;
		coordinates["wght"] = weight == "bold" ? 700 : weight == "normal" ? 400
																		  : weight.to_int();
		isolated->set_variation_opentype(coordinates);
		const int numeric_weight = int(coordinates["wght"]);
		if (numeric_weight > 400 && !r_font.base->get_supported_variation_list().has(TS->name_to_tag("wght"))) {
			isolated->set_variation_embolden((numeric_weight - 400) / 250.0f);
		}
		if (style == "italic") {
			isolated->set_variation_transform(Transform2D(1, 0, -0.2, 1, 0, 0));
		}
		isolated->set_spacing(TextServer::SPACING_GLYPH, int(Math::round(spacing)));
		r_font.font = isolated;
	}
	if (r_font.base.is_valid()) {
		r_font.revision = hash_murmur3_one_64(uint64_t(r_font.base->get_instance_id()), r_font.revision);
	}
	return true;
}
