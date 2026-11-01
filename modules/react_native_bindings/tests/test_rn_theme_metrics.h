#pragma once

#include "../components/rn_theme_metrics.h"

#include "scene/gui/control.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/theme.h"
#include "tests/test_macros.h"

#include <thread>

namespace TestRNThemeMetrics {

TEST_CASE("[ReactNativeBindings][ThemeMetrics][SceneTree] captured metrics publish immutably to other threads") {
	Control *owner = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(owner);
	RNThemeMetricsStore store;
	const auto initial = store.get();
	store.publish(RNThemeMetrics::capture(owner, 3));
	const auto published = store.get();
	CHECK(initial.get() != published.get());
	CHECK(published->revision == 3);
	CHECK(published->text_font.is_valid());
	CHECK(published->switch_size.x > 0);
	CHECK(published->progress_bar_size.y > 0);
	CHECK(owner->get_child_count(true) == 0);

	uint64_t seen = 0;
	std::thread reader([&]() { seen = store.get()->revision; });
	reader.join();
	CHECK(seen == 3);

	store.publish(7, RNThemeMetrics::capture(owner, 4));
	store.publish(7, RNThemeMetrics::capture(owner, 5));
	CHECK(store.get(7)->revision == 4);
	store.publish(RNThemeMetrics::capture(owner, 6));
	CHECK(store.get()->revision == 3);
	Ref<Theme> theme;
	theme.instantiate();
	theme->set_font_size("font_size", "LineEdit", 31);
	owner->set_theme(theme);
	store.publish(7, RNThemeMetrics::capture(owner, 7));
	CHECK(store.get(7)->revision == 7);
	memdelete(owner);
}

TEST_CASE("[ReactNativeBindings][ThemeMetrics][SceneTree] theme type variations capture per class and fall back to the class") {
	Control *owner = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(owner);
	Ref<Theme> theme;
	theme.instantiate();
	theme->set_type_variation("TitleText", "RichTextLabel");
	theme->set_font_size("normal_font_size", "TitleText", 40);
	theme->set_color("default_color", "TitleText", Color(1, 0, 0));
	theme->set_type_variation("LargeBar", "ProgressBar");
	theme->set_font_size("font_size", "LargeBar", 60);
	owner->set_theme(theme);
	const RNThemeMetrics metrics = RNThemeMetrics::capture(owner, 1);
	const RNThemeTypeMetrics &title = metrics.get_type("RichTextLabel", "TitleText");
	CHECK(title.font_size == 40);
	CHECK(title.color == Color(1, 0, 0));
	CHECK(title.rich_fonts.size() == 5);
	CHECK(metrics.get_type("RichTextLabel", "Unknown").font_size == metrics.get_type("RichTextLabel").font_size);
	CHECK(metrics.get_type("ProgressBar", "LargeBar").minimum_size.y > metrics.get_type("ProgressBar").minimum_size.y);
	CHECK(metrics.get_type("HSlider").minimum_size.y > 0);
	CHECK(metrics.get_type("VSlider").minimum_size.x > 0);

	Ref<Theme> changed = theme->duplicate();
	changed->set_font_size("normal_font_size", "TitleText", 41);
	owner->set_theme(changed);
	CHECK_FALSE(RNThemeMetrics::capture(owner, 2).same_theme_as(metrics));
	memdelete(owner);
}

TEST_CASE("[ReactNativeBindings][ThemeMetrics] themed font families carry a theme type and the requested family") {
	CHECK(RNThemedFontFamily::parse("res://font.ttf").theme_type.empty());
	CHECK(RNThemedFontFamily::parse("res://font.ttf").family == "res://font.ttf");
	CHECK(RNThemedFontFamily::parse("godot-theme:Title|").theme_type == "Title");
	CHECK(RNThemedFontFamily::parse("godot-theme:Title|").family.empty());
	CHECK(RNThemedFontFamily::parse("godot-theme:Title|Serif").family == "Serif");
}

} // namespace TestRNThemeMetrics
