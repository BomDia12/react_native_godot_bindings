#pragma once

#include "../components/rn_paragraph_layout.h"

#include "core/os/thread.h"
#include "scene/theme/theme_db.h"
#include "tests/test_macros.h"

namespace TestRNParagraphLayout {

Vector<RNParagraphLayout::Fragment> fragments(const Vector<String> &p_texts, float p_line_height = 0) {
	Vector<RNParagraphLayout::Fragment> result;
	for (const String &text : p_texts) {
		RNParagraphLayout::Fragment fragment;
		fragment.text = text;
		fragment.font = ThemeDB::get_singleton()->get_fallback_font();
		fragment.font_size = 16;
		fragment.line_height = p_line_height;
		result.push_back(fragment);
	}
	return result;
}

RNParagraphLayout::Attributes width(float p_width, int p_max_lines = 0) {
	RNParagraphLayout::Attributes attributes;
	attributes.width = p_width;
	attributes.max_lines = p_max_lines;
	return attributes;
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] words wrap to the width and mandatory breaks always split") {
	RNParagraphLayout layout;
	layout.shape(fragments({ "alpha beta ", "gamma delta" }), width(-1));
	REQUIRE(layout.get_lines().size() == 1);
	const float single = layout.get_size().x;
	CHECK(layout.get_lines()[0].range == Vector2i(0, 22));

	layout.shape(fragments({ "alpha beta ", "gamma delta" }), width(single / 2));
	REQUIRE(layout.get_lines().size() >= 2);
	int next = 0;
	for (const RNParagraphLayout::Line &line : layout.get_lines()) {
		CHECK(line.bounds.size.x <= single / 2);
		CHECK(line.range.x >= next);
		next = line.range.y;
	}
	CHECK(next == 22);

	layout.shape(fragments({ "a\nb" }), width(-1));
	CHECK(layout.get_lines().size() == 2);
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] fixed line heights center glyphs and the tallest fragment sets the box") {
	RNParagraphLayout layout;
	Vector<RNParagraphLayout::Fragment> parts = fragments({ "short ", "tall" }, 30);
	parts.write[1].line_height = 44;
	layout.shape(parts, width(-1));
	REQUIRE(layout.get_lines().size() == 1);
	const RNParagraphLayout::Line &line = layout.get_lines()[0];
	CHECK(line.bounds.size.y == 44);
	CHECK(layout.get_size().y == 44);

	RNParagraphLayout natural;
	natural.shape(fragments({ "short tall" }), width(-1));
	const RNParagraphLayout::Line &natural_line = natural.get_lines()[0];
	CHECK(line.baseline == doctest::Approx(natural_line.baseline + (44 - natural_line.bounds.size.y) / 2));
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] line limits trim the last visible line with an ellipsis") {
	RNParagraphLayout unbounded;
	unbounded.shape(fragments({ "one two three four five six" }), width(-1));
	const float single = unbounded.get_size().x;
	RNParagraphLayout limited;
	limited.shape(fragments({ "one two three four five six" }), width(single / 3, 1));
	REQUIRE(limited.get_lines().size() == 1);
	CHECK(limited.get_size().x <= single / 3);
	CHECK(limited.get_size().y == unbounded.get_size().y);
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] hit testing and range rects follow shaped lines") {
	RNParagraphLayout layout;
	layout.shape(fragments({ "first ", "second" }), width(-1));
	const RNParagraphLayout::Line &line = layout.get_lines()[0];
	CHECK(layout.hit_test(Point2(-10, line.baseline)) == 0);
	CHECK(layout.hit_test(Point2(line.bounds.size.x + 10, line.baseline)) == 12);
	const Vector<Rect2> first = layout.get_range_rects(0, 6);
	const Vector<Rect2> second = layout.get_range_rects(6, 12);
	REQUIRE(first.size() == 1);
	REQUIRE(second.size() == 1);
	CHECK(first[0].position.x == doctest::Approx(0));
	CHECK(second[0].position.x == doctest::Approx(first[0].get_end().x));
	CHECK(second[0].get_end().x <= line.bounds.size.x);
	CHECK(second[0].get_end().x > line.bounds.size.x - 1);
	CHECK(layout.hit_test(second[0].get_center()) >= 6);
	CHECK(layout.get_range_rects(3, 3).is_empty());
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] shaping on a worker thread matches the main thread") {
	struct Work {
		Vector<RNParagraphLayout::Fragment> parts;
		Size2 size;
		int lines = 0;
	};
	Work work;
	work.parts = fragments({ "measured off the main thread" });
	RNParagraphLayout main_layout;
	main_layout.shape(work.parts, width(80));
	Thread thread;
	thread.start([](void *p_work) {
		Work *data = static_cast<Work *>(p_work);
		RNParagraphLayout layout;
		layout.shape(data->parts, width(80));
		data->size = layout.get_size();
		data->lines = layout.get_lines().size();
	},
			&work);
	thread.wait_to_finish();
	CHECK(work.size == main_layout.get_size());
	CHECK(work.lines == main_layout.get_lines().size());
}

} // namespace TestRNParagraphLayout
