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

	struct Fragment {
		String text;
		Ref<Font> font;
		int font_size = 16;
		Color color = Color(1, 1, 1);
		float line_height = 0;
		int decorations = 0;
		Color background = Color(0, 0, 0, 0);
		int tag = 0;
	};

	struct Attributes {
		float width = -1;
		int max_lines = 0;
		TextServer::OverrunBehavior overrun = TextServer::OVERRUN_TRIM_ELLIPSIS;
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

	void _clear();
	int _fragment_at(int p_character) const;
	void _draw_glyphs(RID p_canvas_item, RID p_line, const Point2 &p_origin) const;

public:
	RNParagraphLayout() = default;
	RNParagraphLayout(const RNParagraphLayout &) = delete;
	RNParagraphLayout &operator=(const RNParagraphLayout &) = delete;
	~RNParagraphLayout();

	void shape(const Vector<Fragment> &p_fragments, const Attributes &p_attributes);
	Size2 get_size() const { return size; }
	const Vector<Line> &get_lines() const { return lines; }
	const Vector<Fragment> &get_fragments() const { return fragments; }
	int hit_test(const Point2 &p_point) const;
	Vector<Rect2> get_range_rects(int p_from, int p_to) const;
	void draw(RID p_canvas_item, const Point2 &p_offset) const;
};
