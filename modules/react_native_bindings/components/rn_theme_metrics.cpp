#include "rn_theme_metrics.h"

#include "scene/gui/check_button.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/slider.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

namespace {

Rect2 content_margins(Control *p_owner, const StringName &p_type) {
	const Ref<StyleBox> style = p_owner->get_theme_stylebox(SNAME("normal"), p_type);
	if (style.is_null()) {
		return Rect2();
	}
	return Rect2(style->get_margin(SIDE_LEFT), style->get_margin(SIDE_TOP), style->get_margin(SIDE_RIGHT), style->get_margin(SIDE_BOTTOM));
}

template <typename T>
Size2 native_minimum_size(Control *p_owner, const StringName &p_variation = StringName()) {
	T *probe = memnew(T);
	probe->set_theme_type_variation(p_variation);
	p_owner->add_child(probe, false, Node::INTERNAL_MODE_BACK);
	const Size2 size = probe->get_combined_minimum_size();
	p_owner->remove_child(probe);
	memdelete(probe);
	return size;
}

// Every variation of p_class in the themes p_owner resolves items from.
Vector<StringName> theme_variations(Control *p_owner, const StringName &p_class) {
	List<StringName> names;
	const auto collect = [&](const Ref<Theme> &p_theme) {
		if (p_theme.is_valid()) {
			p_theme->get_type_variation_list(p_class, &names);
		}
	};
	for (Node *node = p_owner; node; node = node->get_parent()) {
		if (const Control *control = Object::cast_to<Control>(node)) {
			collect(control->get_theme());
		} else if (const Window *window = Object::cast_to<Window>(node)) {
			collect(window->get_theme());
		}
	}
	collect(ThemeDB::get_singleton()->get_project_theme());
	collect(ThemeDB::get_singleton()->get_default_theme());
	Vector<StringName> variations = { StringName() };
	for (const StringName &name : names) {
		if (!variations.has(name)) {
			variations.push_back(name);
		}
	}
	return variations;
}

std::string variation_key(const StringName &p_variation) {
	return String(p_variation).utf8().get_data();
}

template <typename T>
void capture_sized_type(RNThemeMetrics &r_metrics, Control *p_owner, const char *p_class) {
	for (const StringName &variation : theme_variations(p_owner, p_class)) {
		r_metrics.types[{ p_class, variation_key(variation) }].minimum_size = native_minimum_size<T>(p_owner, variation);
	}
}

void capture_text_type(RNThemeMetrics &r_metrics, Control *p_owner) {
	const char *text_class = "RichTextLabel";
	for (const StringName &variation : theme_variations(p_owner, text_class)) {
		const StringName type = variation == StringName() ? StringName(text_class) : variation;
		RNThemeTypeMetrics &metrics = r_metrics.types[{ text_class, variation_key(variation) }];
		metrics.font = p_owner->get_theme_font(SNAME("normal_font"), type);
		metrics.font_size = p_owner->get_theme_font_size(SNAME("normal_font_size"), type);
		metrics.color = p_owner->get_theme_color(SNAME("default_color"), type);
		for (const RNRichTextFontItem &item : RN_RICH_TEXT_FONT_ITEMS) {
			metrics.rich_fonts.push_back(p_owner->get_theme_font(item.font, type));
			metrics.rich_font_sizes.push_back(p_owner->get_theme_font_size(item.font_size, type));
		}
		metrics.rich_line_separation = p_owner->get_theme_constant(SNAME("line_separation"), type);
		metrics.rich_paragraph_separation = p_owner->get_theme_constant(SNAME("paragraph_separation"), type);
		metrics.rich_style = p_owner->get_theme_stylebox(SNAME("normal"), type);
	}
}

} // namespace

RNThemedFontFamily RNThemedFontFamily::parse(const std::string &p_font_family) {
	if (!p_font_family.starts_with(PREFIX)) {
		return { "", p_font_family };
	}
	const std::string rest = p_font_family.substr(strlen(PREFIX));
	const size_t separator = rest.find('|');
	return separator == std::string::npos ? RNThemedFontFamily{ rest, "" } : RNThemedFontFamily{ rest.substr(0, separator), rest.substr(separator + 1) };
}

const RNThemeTypeMetrics &RNThemeMetrics::get_type(const char *p_class, const std::string &p_variation) const {
	static const RNThemeTypeMetrics no_metrics;
	auto found = types.find({ p_class, p_variation });
	if (found == types.end()) {
		found = types.find({ p_class, "" });
	}
	return found == types.end() ? no_metrics : found->second;
}

RNThemeMetrics RNThemeMetrics::capture(Control *p_theme_owner, uint64_t p_revision) {
	RNThemeMetrics metrics;
	metrics.revision = p_revision;
	metrics.text_font = p_theme_owner->get_theme_font(SNAME("normal_font"), SNAME("RichTextLabel"));
	metrics.text_font_size = p_theme_owner->get_theme_font_size(SNAME("normal_font_size"), SNAME("RichTextLabel"));
	metrics.text_color = p_theme_owner->get_theme_color(SNAME("default_color"), SNAME("RichTextLabel"));
	metrics.editor_font = p_theme_owner->get_theme_font(SNAME("font"), SNAME("LineEdit"));
	metrics.editor_font_size = p_theme_owner->get_theme_font_size(SNAME("font_size"), SNAME("LineEdit"));
	metrics.line_edit_margins = content_margins(p_theme_owner, SNAME("LineEdit"));
	metrics.text_edit_margins = content_margins(p_theme_owner, SNAME("TextEdit"));
	metrics.switch_size = native_minimum_size<CheckButton>(p_theme_owner);
	metrics.progress_bar_size = native_minimum_size<ProgressBar>(p_theme_owner);
	capture_text_type(metrics, p_theme_owner);
	capture_sized_type<ProgressBar>(metrics, p_theme_owner, "ProgressBar");
	capture_sized_type<HSlider>(metrics, p_theme_owner, "HSlider");
	capture_sized_type<VSlider>(metrics, p_theme_owner, "VSlider");
	return metrics;
}

bool RNThemeMetrics::same_theme_as(const RNThemeMetrics &p_other) const {
	return text_font == p_other.text_font && text_font_size == p_other.text_font_size && text_color == p_other.text_color && editor_font == p_other.editor_font && editor_font_size == p_other.editor_font_size && line_edit_margins == p_other.line_edit_margins && text_edit_margins == p_other.text_edit_margins && switch_size == p_other.switch_size && progress_bar_size == p_other.progress_bar_size && types == p_other.types;
}

void RNThemeMetricsStore::publish(RNThemeMetrics p_metrics) {
	std::lock_guard lock(mutex);
	if (!fallback->same_theme_as(p_metrics)) {
		fallback = std::make_shared<const RNThemeMetrics>(std::move(p_metrics));
	}
}

void RNThemeMetricsStore::publish(int p_surface, RNThemeMetrics p_metrics) {
	std::lock_guard lock(mutex);
	auto found = surfaces.find(p_surface);
	if (found == surfaces.end() || !found->second->same_theme_as(p_metrics)) {
		surfaces[p_surface] = std::make_shared<const RNThemeMetrics>(std::move(p_metrics));
	}
}

void RNThemeMetricsStore::forget(int p_surface) {
	std::lock_guard lock(mutex);
	surfaces.erase(p_surface);
}

std::shared_ptr<const RNThemeMetrics> RNThemeMetricsStore::get(int p_surface) const {
	std::lock_guard lock(mutex);
	auto found = surfaces.find(p_surface);
	return found == surfaces.end() ? fallback : found->second;
}
