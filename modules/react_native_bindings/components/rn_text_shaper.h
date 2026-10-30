#pragma once

#include "rn_paragraph_layout.h"
#include "rn_theme_metrics.h"

#include <react/renderer/attributedstring/AttributedString.h>
#include <react/renderer/attributedstring/ParagraphAttributes.h>

#include <map>
#include <memory>
#include <mutex>
#include <tuple>

class RNMainThreadQueue;

// Converts React attributed strings into RNParagraphLayout input on any thread.
// Fonts that are not cached yet are loaded once on the main thread.
class RNTextShaper {
	// The theme font's ObjectID comes first, as the style applies to it.
	using FontKey = std::tuple<uint64_t, std::string, int, bool, float, int>;

	std::shared_ptr<const RNThemeMetricsStore> theme;
	std::shared_ptr<RNMainThreadQueue> main_thread;
	mutable std::mutex mutex;
	mutable std::map<FontKey, Ref<Font>> fonts;

	Ref<Font> _font(const facebook::react::TextAttributes &p_attributes, const RNThemeMetrics &p_metrics) const;
	void _shape(const facebook::react::AttributedString &p_string, const facebook::react::ParagraphAttributes &p_paragraph, float p_width, float p_scale, facebook::react::SurfaceId p_surface, RNParagraphLayout &r_layout) const;

public:
	static constexpr const char *CONTEXT_KEY = "GodotTextShaper";

	RNTextShaper(std::shared_ptr<const RNThemeMetricsStore> p_theme, std::shared_ptr<RNMainThreadQueue> p_main_thread);

	// p_available may be infinite in either direction. adjustsFontSizeToFit
	// shrinks the text to the largest scale that fits it. The text takes the
	// theme of p_surface.
	void shape(const facebook::react::AttributedString &p_string, const facebook::react::ParagraphAttributes &p_paragraph, const Size2 &p_available, facebook::react::SurfaceId p_surface, RNParagraphLayout &r_layout) const;
	std::shared_ptr<const RNThemeMetrics> get_theme_metrics(facebook::react::SurfaceId p_surface) const { return theme->get(p_surface); }
	// The surface of a paragraph's text. Placeholders and empty strings have
	// none, so measurements take the surface from their layout context.
	static facebook::react::SurfaceId surface_of(const facebook::react::AttributedString &p_string);
	Ref<Font> resolve_font(const facebook::react::TextAttributes &p_attributes, facebook::react::SurfaceId p_surface, int &r_size, Color &r_color) const;
};
