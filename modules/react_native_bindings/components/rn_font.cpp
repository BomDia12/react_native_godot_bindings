#include "rn_font.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"

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

namespace {

Ref<Font> styled_font(const Ref<Font> &p_base, const RNFontStyle &p_style, bool p_top_level) {
	if (p_base.is_null()) {
		return p_base;
	}
	const int64_t weight_tag = TS->name_to_tag("wght");
	const bool variable = p_base->get_supported_variation_list().has(weight_tag);
	// A FontVariation author's own settings stay, and the style only adds to them.
	const Ref<FontVariation> authored = p_base;
	const bool keeps_authored_weight = authored.is_valid() && Dictionary(authored->get_variation_opentype()).has(weight_tag) && !p_style.weight_requested;
	// A variable font's default instance need not be regular, so one with a
	// weight axis gets the requested weight unless its author picked one.
	if (p_style.is_default() && (!variable || keeps_authored_weight)) {
		return p_base;
	}
	Ref<FontVariation> variation;
	if (authored.is_valid()) {
		variation = authored->duplicate();
	} else {
		variation.instantiate();
		variation->set_base_font(p_base);
	}
	Dictionary coordinates = Dictionary(variation->get_variation_opentype()).duplicate();
	if (!keeps_authored_weight) {
		coordinates[weight_tag] = p_style.weight;
	}
	variation->set_variation_opentype(coordinates);
	if (p_style.weight > 400 && !variable) {
		variation->set_variation_embolden(variation->get_variation_embolden() + (p_style.weight - 400) / 250.0f);
	}
	if (p_style.italic) {
		variation->set_variation_transform(variation->get_variation_transform() * Transform2D(1, 0.2, 0, 1, 0, 0));
	}
	variation->set_spacing(TextServer::SPACING_GLYPH, variation->get_spacing(TextServer::SPACING_GLYPH) + int(Math::round(p_style.letter_spacing)));
	Dictionary features = Dictionary(p_base->get_opentype_features()).duplicate();
	for (const String &feature : p_style.features) {
		features[TS->name_to_tag(feature)] = 1;
	}
	variation->set_opentype_features(features);
	// A FontVariation without fallbacks of its own uses its base font's, or the
	// theme's when it has no base. Godot does this only for the font it draws
	// with; fonts in a fallback chain contribute their own fallbacks alone.
	TypedArray<Font> source = p_base->get_fallbacks();
	if (p_top_level && authored.is_valid() && source.is_empty()) {
		if (const Ref<Font> base = authored->_get_base_font_or_default(); base.is_valid()) {
			source = base->get_fallbacks();
		}
	}
	TypedArray<Font> fallbacks;
	for (const Variant &fallback : source) {
		fallbacks.push_back(styled_font(Ref<Font>(fallback), p_style, false));
	}
	variation->set_fallbacks(fallbacks);
	return variation;
}

} // namespace

Ref<Font> rn_font_variation(const Ref<Font> &p_base, const RNFontStyle &p_style) {
	return styled_font(p_base, p_style, true);
}
