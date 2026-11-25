#pragma once

#include "core/math/rect2.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

class Control;

struct RNThemeTypeMetrics {
	Ref<Font> font;
	int font_size = 16;
	Color color = Color(1, 1, 1);
	Size2 minimum_size;
	// What a RichTextLabel needs to lay BBCode out as its mounted label does,
	// in the order of RN_RICH_TEXT_FONT_ITEMS.
	Vector<Ref<Font>> rich_fonts;
	Vector<int> rich_font_sizes;
	int rich_line_separation = 0;
	int rich_paragraph_separation = 0;
	Ref<StyleBox> rich_style;

	bool operator==(const RNThemeTypeMetrics &p_other) const {
		return font == p_other.font && font_size == p_other.font_size && color == p_other.color && minimum_size == p_other.minimum_size && rich_fonts == p_other.rich_fonts && rich_font_sizes == p_other.rich_font_sizes && rich_line_separation == p_other.rich_line_separation && rich_paragraph_separation == p_other.rich_paragraph_separation && rich_style == p_other.rich_style;
	}
};

struct RNRichTextFontItem {
	const char *font;
	const char *font_size;
};
inline constexpr RNRichTextFontItem RN_RICH_TEXT_FONT_ITEMS[] = {
	{ "normal_font", "normal_font_size" },
	{ "bold_font", "bold_font_size" },
	{ "italics_font", "italics_font_size" },
	{ "bold_italics_font", "bold_italics_font_size" },
	{ "mono_font", "mono_font_size" },
};

// A Text's godotThemeType reaches every nested span as this fontFamily, because
// spans inherit text attributes and nothing else from their paragraph. Whatever
// follows the separator is the family the application asked for.
struct RNThemedFontFamily {
	static constexpr const char *PREFIX = "godot-theme:";
	std::string theme_type;
	std::string family;

	static RNThemedFontFamily parse(const std::string &p_font_family);
};

struct RNThemeMetrics {
	Ref<Font> text_font;
	int text_font_size = 16;
	Color text_color = Color(1, 1, 1);
	Ref<Font> editor_font;
	int editor_font_size = 16;
	Rect2 line_edit_margins;
	Rect2 text_edit_margins;
	Size2 switch_size;
	Size2 progress_bar_size;
	uint64_t revision = 0;
	// Keyed by Godot class and theme type variation; "" is the class itself.
	std::map<std::pair<std::string, std::string>, RNThemeTypeMetrics> types;

	// A variation the theme doesn't declare falls back to the class, as in Godot.
	const RNThemeTypeMetrics &get_type(const char *p_class, const std::string &p_variation = "") const;

	static RNThemeMetrics capture(Control *p_theme_owner, uint64_t p_revision);
	bool same_theme_as(const RNThemeMetrics &p_other) const;
};

// The theme each surface measures and draws with, captured from its root, so
// roots under differently themed controls look like their surroundings.
// Surfaces without metrics of their own, and readers without a surface, use
// the fallback. Publishing the theme a surface already has keeps its revision,
// so text measured with it stays cached while its root moves or resizes.
class RNThemeMetricsStore {
	mutable std::mutex mutex;
	std::shared_ptr<const RNThemeMetrics> fallback = std::make_shared<RNThemeMetrics>();
	std::unordered_map<int, std::shared_ptr<const RNThemeMetrics>> surfaces;

public:
	void publish(RNThemeMetrics p_metrics);
	void publish(int p_surface, RNThemeMetrics p_metrics);
	void forget(int p_surface);
	std::shared_ptr<const RNThemeMetrics> get(int p_surface = -1) const;
};
