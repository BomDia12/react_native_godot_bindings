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

} // namespace TestRNThemeMetrics
