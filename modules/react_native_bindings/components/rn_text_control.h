#pragma once

#include "rn_paragraph_layout.h"

#include "scene/gui/control.h"
#include "scene/resources/style_box.h"

#include <functional>

using RNTextShapeFunction = std::function<void(const Size2 &p_size, RNParagraphLayout &r_layout)>;

class RNTextControl : public Control {
	GDCLASS(RNTextControl, Control);

	String text;
	RNTextShapeFunction shape_function;
	bool selectable = false;
	Color selection_color;
	bool has_selection_color = false;
	HorizontalAlignment accessibility_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	RNParagraphLayout layout;
	Ref<StyleBox> box;
	// Padding and border; the paragraph lays out in the content box inside them.
	Point2 content_origin;
	Size2 content_inset;
	int selection_from = -1;
	int selection_to = -1;
	int selection_anchor = -1;
	bool selecting = false;
	uint64_t last_double_click_msec = 0;
	void _shape();
	void _select_word(int p_character);
	void _select_paragraph(int p_character);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNTextControl();
	String get_text() const;
	int get_font_size() const;
	Color get_text_color() const;
	void set_content(const String &p_text, RNTextShapeFunction p_shape_function, bool p_selectable);
	void set_selection_color(const Color &p_color);
	// The Text view's own background, borders and outline, drawn under the text.
	void set_box(const Ref<StyleBox> &p_box);
	void set_content_insets(real_t p_left, real_t p_top, real_t p_right, real_t p_bottom);
	// A point in this control's coordinates, in the paragraph's.
	Point2 to_content(const Point2 &p_local) const { return p_local - content_origin; }
	void set_accessibility_alignment(HorizontalAlignment p_alignment) { accessibility_alignment = p_alignment; }
	const RNParagraphLayout &get_layout() const { return layout; }
	bool is_selectable() const;
	void set_selection_range(int p_from, int p_to);
	int get_selection_from() const { return selection_from; }
	int get_selection_to() const { return selection_to; }
};
