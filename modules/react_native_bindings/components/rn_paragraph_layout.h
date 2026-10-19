#pragma once

#include "core/math/rect2.h"
#include "core/templates/vector.h"
#include "scene/resources/font.h"
#include "servers/text/text_server.h"

class RNParagraphLayout {
public:
	enum Decoration {
		DECORATION_UNDERLINE = 1,
		DECORATION_STRIKETHROUGH = 2,
	};

	enum DecorationStyle {
		DECORATION_SOLID,
		DECORATION_DOUBLE,
		DECORATION_DOTTED,
		DECORATION_DASHED,
	};

	enum EllipsisPosition {
		ELLIPSIS_TAIL,
		ELLIPSIS_HEAD,
		ELLIPSIS_MIDDLE,
	};

	struct Fragment {
		String text;
		Ref<Font> font;
		int font_size = 16;
		Color color = Color(1, 1, 1);
		float line_height = 0;
		int decorations = 0;
		Color decoration_color = Color(1, 1, 1);
		DecorationStyle decoration_style = DECORATION_SOLID;
		Color background = Color(0, 0, 0, 0);
		Color shadow_color = Color(0, 0, 0, 0);
		Vector2 shadow_offset;
		float shadow_radius = 0;
		int tag = 0;
	};

	struct Attributes {
		float width = -1;
		int max_lines = 0;
		TextServer::OverrunBehavior overrun = TextServer::OVERRUN_TRIM_ELLIPSIS;
		EllipsisPosition ellipsis = ELLIPSIS_TAIL;
		HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_LEFT;
		TextServer::Direction direction = TextServer::DIRECTION_AUTO;
	};

	struct Line {
		Vector2i range;
		Rect2 bounds;
		float baseline = 0;
	};

private:
	Vector<Fragment> fragments;
	Vector<Vector2i> fragment_ranges;
	RID paragraph;
	Vector<RID> line_rids;
	Vector<Line> lines;
	Size2 size;
	bool truncated = false;
	// Characters a head or middle ellipsis hides, in the original text. Layout
	// indices after the ellipsis are shifted by the hidden length minus one.
	Vector2i elided = Vector2i(-1, -1);

	void _clear();
	void _shape(const Vector<Fragment> &p_fragments, const Attributes &p_attributes);
	bool _elide(const Vector<Fragment> &p_fragments, const Attributes &p_attributes);
	int _to_layout(int p_index) const;
	int _to_original(int p_index) const;
	void _draw_glyphs(RID p_canvas_item, RID p_line, const Point2 &p_origin, bool p_shadows) const;
	void _draw_decoration(RID p_canvas_item, const Fragment &p_fragment, float p_x, float p_y, float p_width, float p_thickness) const;

public:
	RNParagraphLayout() = default;
	RNParagraphLayout(const RNParagraphLayout &) = delete;
	RNParagraphLayout &operator=(const RNParagraphLayout &) = delete;
	~RNParagraphLayout();

	void shape(const Vector<Fragment> &p_fragments, const Attributes &p_attributes);
	Size2 get_size() const { return size; }
	const Vector<Line> &get_lines() const { return lines; }
	const Vector<Fragment> &get_fragments() const { return fragments; }
	// The fragment drawn at a character of the shaped text.
	int fragment_at(int p_character) const;
	// More lines than max_lines, or a line wider than the width.
	bool is_truncated() const { return truncated; }
	Vector2i get_elided_range() const { return elided; }
	int hit_test(const Point2 &p_point) const;
	Vector<Rect2> get_range_rects(int p_from, int p_to) const;
	void draw(RID p_canvas_item, const Point2 &p_offset) const;
};
