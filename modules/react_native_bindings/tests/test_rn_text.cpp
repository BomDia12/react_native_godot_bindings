#include "../components/rn_font.h"
#include "../components/rn_text_shaper.h"
#include "../host/rn_main_thread_queue.h"
#include "support/rn_test_fonts.h"
#include "support/rn_upstream_test_macros.h"

#include "scene/theme/theme_db.h"

namespace TestRNText {

using namespace facebook::react;

struct Shaper {
	std::shared_ptr<RNThemeMetricsStore> theme = std::make_shared<RNThemeMetricsStore>();
	std::shared_ptr<RNMainThreadQueue> main_thread = std::make_shared<RNMainThreadQueue>();
	RNTextShaper shaper{ theme, main_thread };

	Shaper() {
		RNThemeMetrics metrics;
		metrics.text_font = ThemeDB::get_singleton()->get_fallback_font();
		metrics.text_font_size = 16;
		theme->publish(metrics);
	}
};

AttributedString attributed(const std::string &p_text, const std::function<void(TextAttributes &)> &p_change = nullptr) {
	AttributedString::Fragment fragment;
	fragment.string = p_text;
	fragment.textAttributes = TextAttributes::defaultTextAttributes();
	fragment.textAttributes.fontSize = 16;
	if (p_change) {
		p_change(fragment.textAttributes);
	}
	AttributedString result;
	result.appendFragment(std::move(fragment));
	return result;
}

const Size2 UNBOUNDED(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());

TEST_CASE("[ReactNativeBindings][Text][SceneTree] natural, start and end alignment follow the layout direction") {
	Shaper shaper;
	RNParagraphLayout layout;
	shaper.shaper.shape(attributed("aligned", [](TextAttributes &p_attributes) { p_attributes.layoutDirection = LayoutDirection::RightToLeft; }), ParagraphAttributes(), Size2(300, 100), layout);
	CHECK(layout.get_lines()[0].bounds.get_end().x == doctest::Approx(300));

	shaper.shaper.shape(attributed("aligned", [](TextAttributes &p_attributes) { p_attributes.alignment = TextAlignment::End; }), ParagraphAttributes(), Size2(300, 100), layout);
	CHECK(layout.get_lines()[0].bounds.get_end().x == doctest::Approx(300));

	shaper.shaper.shape(attributed("aligned", [](TextAttributes &p_attributes) {
		p_attributes.layoutDirection = LayoutDirection::RightToLeft;
		p_attributes.alignment = TextAlignment::Left;
	}),
			ParagraphAttributes(), Size2(300, 100), layout);
	CHECK(layout.get_lines()[0].bounds.position.x == 0);
}

TEST_CASE("[ReactNativeBindings][Text][SceneTree] text transforms keep character indices") {
	Shaper shaper;
	RNParagraphLayout layout;
	shaper.shaper.shape(attributed("hello big world", [](TextAttributes &p_attributes) { p_attributes.textTransform = TextTransform::Capitalize; }), ParagraphAttributes(), UNBOUNDED, layout);
	CHECK(layout.get_fragments()[0].text == "Hello Big World");
	shaper.shaper.shape(attributed("straße", [](TextAttributes &p_attributes) { p_attributes.textTransform = TextTransform::Uppercase; }), ParagraphAttributes(), UNBOUNDED, layout);
	CHECK(layout.get_fragments()[0].text.length() == String::utf8("straße").length());
}

TEST_CASE("[ReactNativeBindings][Text][SceneTree] decorations and shadows take their own colors and styles") {
	Shaper shaper;
	RNParagraphLayout layout;
	const SharedColor red = colorFromComponents({ .red = 1, .green = 0, .blue = 0, .alpha = 1 });
	shaper.shaper.shape(attributed("decorated", [&red](TextAttributes &p_attributes) {
		p_attributes.foregroundColor = red;
		p_attributes.textDecorationLineType = TextDecorationLineType::Underline;
		p_attributes.textDecorationStyle = TextDecorationStyle::Dashed;
	}),
			ParagraphAttributes(), UNBOUNDED, layout);
	const RNParagraphLayout::Fragment &defaulted = layout.get_fragments()[0];
	CHECK(defaulted.decoration_color.is_equal_approx(::Color(1, 0, 0)));
	CHECK(defaulted.decoration_style == RNParagraphLayout::DECORATION_DASHED);
	CHECK(defaulted.shadow_color.a == 0);

	shaper.shaper.shape(attributed("decorated", [](TextAttributes &p_attributes) {
		p_attributes.textDecorationColor = colorFromComponents({ .red = 0, .green = 0, .blue = 1, .alpha = 1 });
		p_attributes.textShadowOffset = Size{ .width = 2, .height = 3 };
		p_attributes.textShadowRadius = 4;
	}),
			ParagraphAttributes(), UNBOUNDED, layout);
	const RNParagraphLayout::Fragment &styled = layout.get_fragments()[0];
	CHECK(styled.decoration_color.is_equal_approx(::Color(0, 0, 1)));
	CHECK(styled.shadow_offset == Vector2(2, 3));
	CHECK(styled.shadow_radius == 4);
	CHECK(styled.shadow_color.a > 0);
}

TEST_CASE("[ReactNativeBindings][Text][SceneTree] adjustsFontSizeToFit picks the largest scale that fits") {
	Shaper shaper;
	ParagraphAttributes paragraph;
	paragraph.adjustsFontSizeToFit = true;
	paragraph.maximumNumberOfLines = 1;
	const AttributedString text = attributed("a sentence that is far too long for its box");
	RNParagraphLayout natural;
	shaper.shaper.shape(text, ParagraphAttributes(), UNBOUNDED, natural);
	const float width = natural.get_size().x / 2;

	RNParagraphLayout fitted;
	shaper.shaper.shape(text, paragraph, Size2(width, 100), fitted);
	CHECK_FALSE(fitted.is_truncated());
	CHECK(fitted.get_size().x <= width);
	const int size = fitted.get_fragments()[0].font_size;
	CHECK(size < 16);
	CHECK(size >= 7);

	paragraph.minimumFontScale = 0.9;
	shaper.shaper.shape(text, paragraph, Size2(width, 100), fitted);
	CHECK(fitted.get_fragments()[0].font_size == 14);
	CHECK(fitted.is_truncated());
}

TEST_CASE("[ReactNativeBindings][Text][SceneTree] weight, style and features apply to fallback fonts") {
	Ref<FontVariation> base;
	base.instantiate();
	base->set_base_font(ThemeDB::get_singleton()->get_fallback_font());
	base->set_fallbacks({ rn_test_font("NotoSansArabic.ttf") });
	RNFontStyle style;
	style.weight = 700;
	style.italic = true;
	style.features = { "tnum" };
	const Ref<FontVariation> styled = rn_font_variation(base, style);
	REQUIRE(styled.is_valid());
	REQUIRE(styled->get_fallbacks().size() == 1);
	const Ref<FontVariation> fallback = styled->get_fallbacks()[0];
	REQUIRE(fallback.is_valid());
	CHECK(int(Dictionary(fallback->get_variation_opentype())["wght"]) == 700);
	CHECK(fallback->get_variation_transform() != Transform2D());
	CHECK(fallback->get_opentype_features().has(TS->name_to_tag("tnum")));
	CHECK(rn_font_variation(base, RNFontStyle()) == base);
}

} // namespace TestRNText

void rn_force_link_text_tests() {}
