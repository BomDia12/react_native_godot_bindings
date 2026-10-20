#pragma once

#include "../components/rn_paragraph_layout.h"
#include "support/rn_test_fonts.h"

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

// Hebrew and Arabic runs come from the fixture fonts, as fallbacks of the theme font.
Vector<RNParagraphLayout::Fragment> scripts(const Vector<String> &p_texts) {
	Ref<FontVariation> font;
	font.instantiate();
	font->set_base_font(ThemeDB::get_singleton()->get_fallback_font());
	font->set_fallbacks({ rn_test_font("NotoSansHebrew.ttf"), rn_test_font("NotoSansArabic.ttf") });
	Vector<RNParagraphLayout::Fragment> result = fragments(p_texts);
	for (RNParagraphLayout::Fragment &fragment : result) {
		fragment.font = font;
	}
	return result;
}

// Shapes on a worker thread, as Fabric measures text.
void shape_off_main_thread(RNParagraphLayout &r_layout, const Vector<RNParagraphLayout::Fragment> &p_fragments, const RNParagraphLayout::Attributes &p_attributes) {
	struct Work {
		RNParagraphLayout *layout;
		const Vector<RNParagraphLayout::Fragment> *fragments;
		const RNParagraphLayout::Attributes *attributes;
	} work{ &r_layout, &p_fragments, &p_attributes };
	Thread thread;
	thread.start([](void *p_work) {
		Work *data = static_cast<Work *>(p_work);
		data->layout->shape(*data->fragments, *data->attributes);
	},
			&work);
	thread.wait_to_finish();
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] right-to-left paragraphs keep the requested alignment and hit-test by logical index") {
	REQUIRE(rn_test_font("NotoSansHebrew.ttf").is_valid());
	const String hebrew = String::utf8("שלום עולם");
	RNParagraphLayout layout;
	shape_off_main_thread(layout, scripts({ hebrew }), width(400));
	REQUIRE(layout.get_lines().size() == 1);
	const RNParagraphLayout::Line &line = layout.get_lines()[0];
	CHECK(line.bounds.position.x == 0);
	// The first character of a right-to-left run sits at its right edge.
	CHECK(layout.hit_test(Point2(line.bounds.size.x - 1, line.baseline)) == 0);
	CHECK(layout.hit_test(Point2(1, line.baseline)) == hebrew.length());

	RNParagraphLayout::Attributes right = width(400);
	right.alignment = HORIZONTAL_ALIGNMENT_RIGHT;
	shape_off_main_thread(layout, scripts({ hebrew }), right);
	CHECK(layout.get_lines()[0].bounds.get_end().x == doctest::Approx(400));
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] selection across a direction change splits into visual runs") {
	const String mixed = String::utf8("abc שלום def");
	RNParagraphLayout layout;
	RNParagraphLayout::Attributes attributes = width(-1);
	attributes.direction = TextServer::DIRECTION_LTR;
	shape_off_main_thread(layout, scripts({ mixed }), attributes);
	const Vector<Rect2> whole = layout.get_range_rects(0, mixed.length());
	const Vector<Rect2> crossing = layout.get_range_rects(2, 6);
	CHECK(whole.size() >= 1);
	CHECK(crossing.size() >= 2);
	const RNParagraphLayout::Line &line = layout.get_lines()[0];
	CHECK(layout.hit_test(Point2(-5, line.baseline)) == 0);
	CHECK(layout.hit_test(Point2(line.bounds.size.x + 5, line.baseline)) == mixed.length());
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] Arabic tail ellipsis stays within the width") {
	REQUIRE(rn_test_font("NotoSansArabic.ttf").is_valid());
	const String arabic = String::utf8("مرحبا بالعالم من محرك جودو");
	RNParagraphLayout unbounded;
	unbounded.shape(scripts({ arabic }), width(-1));
	RNParagraphLayout limited;
	shape_off_main_thread(limited, scripts({ arabic }), width(unbounded.get_size().x / 2, 1));
	REQUIRE(limited.get_lines().size() == 1);
	CHECK(limited.is_truncated());
	// Glyph advances round, so the trimmed line may pass the width by under a pixel.
	CHECK(limited.get_size().x <= unbounded.get_size().x / 2 + 1);
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] head and middle ellipses keep the ends and map indices back") {
	const String text = "abcdefghijklmnopqrstuvwxyz0123456789";
	RNParagraphLayout unbounded;
	unbounded.shape(fragments({ text }), width(-1));
	const float half = unbounded.get_size().x / 2;

	RNParagraphLayout::Attributes head = width(half, 1);
	head.ellipsis = RNParagraphLayout::ELLIPSIS_HEAD;
	RNParagraphLayout layout;
	layout.shape(fragments({ text }), head);
	REQUIRE(layout.get_lines().size() == 1);
	CHECK(layout.get_size().x <= half);
	CHECK(layout.get_elided_range().x == 0);
	CHECK(layout.get_elided_range().y > 0);
	CHECK(layout.get_fragments()[0].text == String::chr(0x2026));
	const RNParagraphLayout::Line &line = layout.get_lines()[0];
	CHECK(layout.hit_test(Point2(line.bounds.size.x + 5, line.baseline)) == text.length());

	RNParagraphLayout::Attributes middle = width(half, 1);
	middle.ellipsis = RNParagraphLayout::ELLIPSIS_MIDDLE;
	layout.shape(fragments({ text }), middle);
	const Vector2i elided = layout.get_elided_range();
	CHECK(elided.x > 0);
	CHECK(elided.y < text.length());
	CHECK(layout.get_size().x <= half);
	CHECK(layout.hit_test(Point2(-5, layout.get_lines()[0].baseline)) == 0);
	// A range inside the hidden text highlights the ellipsis.
	CHECK(layout.get_range_rects(elided.x, elided.x + 1).size() == 1);

	RNParagraphLayout::Attributes multiline = width(half, 2);
	multiline.ellipsis = RNParagraphLayout::ELLIPSIS_HEAD;
	layout.shape(fragments({ text }), multiline);
	CHECK(layout.get_elided_range() == Vector2i(-1, -1));
}

TEST_CASE("[ReactNativeBindings][ParagraphLayout][SceneTree] a trailing newline starts an empty last line") {
	RNParagraphLayout layout;
	layout.shape(fragments({ "one\n" }), width(-1));
	REQUIRE(layout.get_lines().size() == 2);
	const RNParagraphLayout::Line &empty = layout.get_lines()[1];
	CHECK(empty.range == Vector2i(4, 4));
	CHECK(empty.bounds.size.y == doctest::Approx(layout.get_lines()[0].bounds.size.y));
	CHECK(layout.hit_test(Point2(0, empty.bounds.get_center().y)) == 4);
	layout.shape(fragments({ "one\n" }), width(-1, 1));
	CHECK(layout.get_lines().size() == 1);
}

} // namespace TestRNParagraphLayout
