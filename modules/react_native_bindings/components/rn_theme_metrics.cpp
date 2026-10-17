#include "rn_theme_metrics.h"

#include "scene/gui/check_button.h"
#include "scene/gui/progress_bar.h"

namespace {

Rect2 content_margins(Control *p_owner, const StringName &p_type) {
	const Ref<StyleBox> style = p_owner->get_theme_stylebox(SNAME("normal"), p_type);
	if (style.is_null()) {
		return Rect2();
	}
	return Rect2(style->get_margin(SIDE_LEFT), style->get_margin(SIDE_TOP), style->get_margin(SIDE_RIGHT), style->get_margin(SIDE_BOTTOM));
}

template <typename T>
Size2 native_minimum_size(Control *p_owner) {
	T *probe = memnew(T);
	p_owner->add_child(probe, false, Node::INTERNAL_MODE_BACK);
	const Size2 size = probe->get_combined_minimum_size();
	p_owner->remove_child(probe);
	memdelete(probe);
	return size;
}

} // namespace

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
	return metrics;
}

void RNThemeMetricsStore::publish(RNThemeMetrics p_metrics) {
	auto published = std::make_shared<const RNThemeMetrics>(std::move(p_metrics));
	std::lock_guard lock(mutex);
	current = std::move(published);
}

std::shared_ptr<const RNThemeMetrics> RNThemeMetricsStore::get() const {
	std::lock_guard lock(mutex);
	return current;
}
