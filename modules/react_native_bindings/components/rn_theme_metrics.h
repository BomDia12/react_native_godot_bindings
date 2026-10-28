#pragma once

#include "core/math/rect2.h"
#include "scene/resources/font.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class Control;

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

	static RNThemeMetrics capture(Control *p_theme_owner, uint64_t p_revision);
};

// The theme each surface measures and draws with, captured from its root, so
// roots under differently themed controls look like their surroundings.
// Surfaces without metrics of their own, and readers without a surface, use
// the fallback.
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
