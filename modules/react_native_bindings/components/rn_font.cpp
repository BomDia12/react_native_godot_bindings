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

Ref<Font> rn_font_variation(const Ref<Font> &p_base, const RNFontStyle &p_style) {
	if (p_base.is_null() || p_style.is_default()) {
		return p_base;
	}
	Ref<FontVariation> variation;
	variation.instantiate();
	variation->set_base_font(p_base);
	Dictionary coordinates;
	coordinates["wght"] = p_style.weight;
	variation->set_variation_opentype(coordinates);
	if (p_style.weight > 400 && !p_base->get_supported_variation_list().has(TS->name_to_tag("wght"))) {
		variation->set_variation_embolden((p_style.weight - 400) / 250.0f);
	}
	if (p_style.italic) {
		variation->set_variation_transform(Transform2D(1, 0.2, 0, 1, 0, 0));
	}
	variation->set_spacing(TextServer::SPACING_GLYPH, int(Math::round(p_style.letter_spacing)));
	Dictionary features = p_base->get_opentype_features();
	for (const String &feature : p_style.features) {
		features[TS->name_to_tag(feature)] = 1;
	}
	variation->set_opentype_features(features);
	TypedArray<Font> fallbacks;
	for (const Variant &fallback : p_base->get_fallbacks()) {
		fallbacks.push_back(rn_font_variation(Ref<Font>(fallback), p_style));
	}
	variation->set_fallbacks(fallbacks);
	return variation;
}
