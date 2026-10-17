#pragma once

#include "rn_paragraph_layout.h"

#include "scene/gui/control.h"

#include <functional>

using RNTextShapeFunction = std::function<void(float p_width, RNParagraphLayout &r_layout)>;

class RNTextControl : public Control {
	GDCLASS(RNTextControl, Control);

	String text;
	RNTextShapeFunction shape_function;
	bool selectable = false;
	Color selection_color;
	bool has_selection_color = false;
	HorizontalAlignment accessibility_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	RNParagraphLayout layout;
	int selection_from = -1;
	int selection_to = -1;
	int selection_anchor = -1;
	bool selecting = false;
	void _shape();

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
	void set_accessibility_alignment(HorizontalAlignment p_alignment) { accessibility_alignment = p_alignment; }
	const RNParagraphLayout &get_layout() const { return layout; }
	bool is_selectable() const;
	void set_selection_range(int p_from, int p_to);
	int get_selection_from() const { return selection_from; }
	int get_selection_to() const { return selection_to; }
};
