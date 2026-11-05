#include "rn_rich_text_measurer.h"

#include "../host/rn_game_services.h"
#include "../host/rn_main_thread_queue.h"

#include "core/io/resource_loader.h"
#include "scene/gui/rich_text_effect.h"
#include "scene/gui/rich_text_label.h"

#include <cmath>

namespace {

constexpr float UNBOUNDED_WIDTH = 1048576;
constexpr size_t RECENT_MEASUREMENTS = 64;

void apply_theme(RichTextLabel *p_label, const RNThemeTypeMetrics &p_type) {
	for (int i = 0; i < p_type.rich_fonts.size(); ++i) {
		p_label->add_theme_font_override(RN_RICH_TEXT_FONT_ITEMS[i].font, p_type.rich_fonts[i]);
		p_label->add_theme_font_size_override(RN_RICH_TEXT_FONT_ITEMS[i].font_size, p_type.rich_font_sizes[i]);
	}
	p_label->add_theme_constant_override(SNAME("line_separation"), p_type.rich_line_separation);
	p_label->add_theme_constant_override(SNAME("paragraph_separation"), p_type.rich_paragraph_separation);
	if (p_type.rich_style.is_valid()) {
		p_label->add_theme_style_override(SNAME("normal"), p_type.rich_style);
	} else {
		p_label->remove_theme_style_override(SNAME("normal"));
	}
}

} // namespace

RNRichTextMeasurer::RNRichTextMeasurer(std::shared_ptr<RNMainThreadQueue> p_main_thread, std::shared_ptr<RNGameServicesHandle> p_services) :
		main_thread(std::move(p_main_thread)), services(std::move(p_services)) {
}

Size2 RNRichTextMeasurer::measure(const String &p_bbcode, const std::vector<std::string> &p_effects, const std::string &p_theme_type, const std::shared_ptr<const RNThemeMetrics> &p_metrics, float p_max_width) {
	const float width = std::isfinite(p_max_width) ? p_max_width : UNBOUNDED_WIDTH;
	const Key key(p_bbcode.utf8().get_data(), p_effects, p_theme_type, width, p_metrics->revision);
	{
		std::lock_guard lock(mutex);
		for (auto entry = recent.begin(); entry != recent.end(); ++entry) {
			if (entry->key == key) {
				recent.splice(recent.begin(), recent, entry);
				return recent.front().size;
			}
		}
	}
	Size2 size;
	const RNThemeTypeMetrics &type = p_metrics->get_type("RichTextLabel", p_theme_type);
	main_thread->run_sync([&] {
		RNGameServices *node = services->get();
		if (!node) {
			return;
		}
		RichTextLabel *label = node->get_measuring_label();
		apply_theme(label, type);
		label->set_size(Size2(width, 1));
		// Tags of effects that aren't installed count as literal text.
		if (installed_effects != p_effects) {
			installed_effects = p_effects;
			Array effects;
			for (const std::string &path : p_effects) {
				const Ref<RichTextEffect> effect = ResourceLoader::load(String::utf8(path.c_str()));
				if (effect.is_valid()) {
					effects.push_back(effect);
				}
			}
			label->set_effects(effects);
			label->set_text(String());
		}
		label->set_text(p_bbcode);
		const Size2 margins = type.rich_style.is_valid() ? type.rich_style->get_minimum_size() : Size2();
		size = Size2(label->get_content_width(), label->get_content_height()) + margins;
	});
	std::lock_guard lock(mutex);
	recent.push_front({ key, size });
	if (recent.size() > RECENT_MEASUREMENTS) {
		recent.pop_back();
	}
	return size;
}
