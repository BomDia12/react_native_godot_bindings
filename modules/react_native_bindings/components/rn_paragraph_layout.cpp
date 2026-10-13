#include "rn_paragraph_layout.h"

#include "servers/rendering/rendering_server.h"

namespace {

constexpr float UNBOUNDED_WIDTH = 1.0e7f;

BitField<TextServer::TextOverrunFlag> overrun_flags(TextServer::OverrunBehavior p_behavior) {
	switch (p_behavior) {
		case TextServer::OVERRUN_NO_TRIMMING:
			return TextServer::OVERRUN_NO_TRIM;
		case TextServer::OVERRUN_TRIM_CHAR:
			return TextServer::OVERRUN_TRIM;
		case TextServer::OVERRUN_TRIM_WORD:
			return TextServer::OVERRUN_TRIM | TextServer::OVERRUN_TRIM_WORD_ONLY;
		case TextServer::OVERRUN_TRIM_WORD_ELLIPSIS:
			return TextServer::OVERRUN_TRIM | TextServer::OVERRUN_TRIM_WORD_ONLY | TextServer::OVERRUN_ADD_ELLIPSIS;
		default:
			return TextServer::OVERRUN_TRIM | TextServer::OVERRUN_ADD_ELLIPSIS;
	}
}

} // namespace

RNParagraphLayout::~RNParagraphLayout() {
	_clear();
}

void RNParagraphLayout::_clear() {
	for (const RID &line : line_rids) {
		TS->free_rid(line);
	}
	if (paragraph.is_valid()) {
		TS->free_rid(paragraph);
	}
	paragraph = RID();
	line_rids.clear();
	lines.clear();
	fragments.clear();
	fragment_ranges.clear();
	size = Size2();
}

int RNParagraphLayout::_fragment_at(int p_character) const {
	for (int i = 0; i < fragment_ranges.size(); ++i) {
		if (p_character >= fragment_ranges[i].x && p_character < fragment_ranges[i].y) {
			return i;
		}
	}
	return fragments.size() - 1;
}

void RNParagraphLayout::shape(const Vector<Fragment> &p_fragments, const Attributes &p_attributes) {
	_clear();
	fragments = p_fragments;
	paragraph = TS->create_shaped_text(p_attributes.direction);
	int offset = 0;
	for (const Fragment &fragment : fragments) {
		const bool shaped = fragment.font.is_valid() && !fragment.text.is_empty() && TS->shaped_text_add_string(paragraph, fragment.text, fragment.font->get_rids(), fragment.font_size, fragment.font->get_opentype_features());
		const int length = shaped ? fragment.text.length() : 0;
		fragment_ranges.push_back(Vector2i(offset, offset + length));
		offset += length;
	}
	if (offset == 0) {
		return;
	}

	const bool wraps = p_attributes.width > 0;
	const BitField<TextServer::LineBreakFlag> break_flags = wraps ? (TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_TRIM_START_EDGE_SPACES | TextServer::BREAK_TRIM_END_EDGE_SPACES) : (TextServer::BREAK_MANDATORY | TextServer::BREAK_TRIM_END_EDGE_SPACES);
	const PackedInt32Array breaks = TS->shaped_text_get_line_breaks(paragraph, wraps ? p_attributes.width : UNBOUNDED_WIDTH, 0, break_flags);
	int line_count = breaks.size() / 2;
	const bool truncated = p_attributes.max_lines > 0 && line_count > p_attributes.max_lines;
	if (truncated) {
		line_count = p_attributes.max_lines;
	}
	for (int i = 0; i < line_count; ++i) {
		const Vector2i range(breaks[i * 2], breaks[i * 2 + 1]);
		const RID line = TS->shaped_text_substr(paragraph, range.x, range.y - range.x);
		const bool last = i == line_count - 1;
		if (wraps && p_attributes.alignment == HORIZONTAL_ALIGNMENT_FILL && !last) {
			TS->shaped_text_fit_to_width(line, p_attributes.width, TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA);
		}
		if (last && (truncated || (wraps && TS->shaped_text_get_width(line) > p_attributes.width))) {
			BitField<TextServer::TextOverrunFlag> flags = overrun_flags(p_attributes.overrun);
			if (truncated) {
				flags.set_flag(TextServer::OVERRUN_ENFORCE_ELLIPSIS);
			}
			TS->shaped_text_overrun_trim_to_width(line, wraps ? p_attributes.width : TS->shaped_text_get_width(line), flags);
		}
		line_rids.push_back(line);

		const float ascent = TS->shaped_text_get_ascent(line);
		const float descent = TS->shaped_text_get_descent(line);
		float line_height = 0;
		for (int j = 0; j < fragments.size(); ++j) {
			if (fragment_ranges[j].x < range.y && fragment_ranges[j].y > range.x) {
				line_height = MAX(line_height, fragments[j].line_height);
			}
		}
		const float height = line_height > 0 ? line_height : ascent + descent;
		Line record;
		record.range = range;
		record.bounds = Rect2(0, size.y, TS->shaped_text_get_width(line), height);
		record.baseline = size.y + ascent + (height - ascent - descent) / 2;
		lines.push_back(record);
		size = Size2(MAX(size.x, record.bounds.size.x), size.y + height);
	}

	const bool rtl = TS->shaped_text_get_inferred_direction(paragraph) == TextServer::DIRECTION_RTL;
	HorizontalAlignment alignment = p_attributes.alignment;
	if (rtl && alignment != HORIZONTAL_ALIGNMENT_CENTER) {
		alignment = alignment == HORIZONTAL_ALIGNMENT_RIGHT ? HORIZONTAL_ALIGNMENT_LEFT : HORIZONTAL_ALIGNMENT_RIGHT;
	}
	const float container = wraps ? p_attributes.width : size.x;
	for (Line &line : lines) {
		const float free_space = container - line.bounds.size.x;
		if (alignment == HORIZONTAL_ALIGNMENT_CENTER) {
			line.bounds.position.x = Math::floor(free_space / 2);
		} else if (alignment == HORIZONTAL_ALIGNMENT_RIGHT) {
			line.bounds.position.x = free_space;
		}
	}
}

int RNParagraphLayout::hit_test(const Point2 &p_point) const {
	if (lines.is_empty()) {
		return 0;
	}
	int index = 0;
	while (index + 1 < lines.size() && p_point.y >= lines[index].bounds.get_end().y) {
		++index;
	}
	return TS->shaped_text_hit_test_position(line_rids[index], p_point.x - lines[index].bounds.position.x);
}

Vector<Rect2> RNParagraphLayout::get_range_rects(int p_from, int p_to) const {
	Vector<Rect2> result;
	for (int i = 0; i < lines.size(); ++i) {
		const Line &line = lines[i];
		const int start = MAX(p_from, line.range.x);
		const int end = MIN(p_to, line.range.y);
		if (end <= start) {
			continue;
		}
		for (const Vector2 &segment : TS->shaped_text_get_selection(line_rids[i], start, end)) {
			result.push_back(Rect2(line.bounds.position.x + segment.x, line.bounds.position.y, segment.y - segment.x, line.bounds.size.y));
		}
	}
	return result;
}

void RNParagraphLayout::_draw_glyphs(RID p_canvas_item, RID p_line, const Point2 &p_origin) const {
	const bool rtl = TS->shaped_text_get_direction(p_line) == TextServer::DIRECTION_RTL;
	const int trim = TS->shaped_text_get_trim_pos(p_line);
	const int ellipsis_position = TS->shaped_text_get_ellipsis_pos(p_line);
	const Glyph *ellipsis = TS->shaped_text_get_ellipsis_glyphs(p_line);
	const int ellipsis_count = TS->shaped_text_get_ellipsis_glyph_count(p_line);
	const Glyph *glyphs = TS->shaped_text_get_glyphs(p_line);
	const int count = TS->shaped_text_get_glyph_count(p_line);
	Color ellipsis_color = fragments.is_empty() ? Color(1, 1, 1) : fragments[fragments.size() - 1].color;
	Vector2 offset = p_origin;

	auto draw_ellipsis = [&]() {
		for (int i = 0; i < ellipsis_count; ++i) {
			const int glyph = rtl ? ellipsis_count - 1 - i : i;
			for (int repeat = 0; repeat < ellipsis[glyph].repeat; ++repeat) {
				TS->font_draw_glyph(ellipsis[glyph].font_rid, p_canvas_item, ellipsis[glyph].font_size, offset + Vector2(ellipsis[glyph].x_off, ellipsis[glyph].y_off), ellipsis[glyph].index, ellipsis_color);
				offset.x += ellipsis[glyph].advance;
			}
		}
	};

	if (rtl && ellipsis_position >= 0) {
		draw_ellipsis();
	}
	for (int i = 0; i < count; ++i) {
		if (trim >= 0 && (rtl ? i < trim : i >= trim)) {
			continue;
		}
		const Color color = fragments[_fragment_at(glyphs[i].start)].color;
		ellipsis_color = color;
		for (int repeat = 0; repeat < glyphs[i].repeat; ++repeat) {
			if (glyphs[i].font_rid.is_valid()) {
				TS->font_draw_glyph(glyphs[i].font_rid, p_canvas_item, glyphs[i].font_size, offset + Vector2(glyphs[i].x_off, glyphs[i].y_off), glyphs[i].index, color);
			}
			offset.x += glyphs[i].advance;
		}
	}
	if (!rtl && ellipsis_position >= 0) {
		draw_ellipsis();
	}
}

void RNParagraphLayout::draw(RID p_canvas_item, const Point2 &p_offset) const {
	RenderingServer *rendering = RenderingServer::get_singleton();
	for (int i = 0; i < lines.size(); ++i) {
		const Line &line = lines[i];
		const RID line_rid = line_rids[i];
		const float ascent = TS->shaped_text_get_ascent(line_rid);
		const float descent = TS->shaped_text_get_descent(line_rid);
		const float underline = TS->shaped_text_get_underline_position(line_rid);
		const float thickness = MAX(1.0, TS->shaped_text_get_underline_thickness(line_rid));
		for (int j = 0; j < fragments.size(); ++j) {
			const Fragment &fragment = fragments[j];
			if (fragment.background.a <= 0 && fragment.decorations == 0) {
				continue;
			}
			const int start = MAX(fragment_ranges[j].x, line.range.x);
			const int end = MIN(fragment_ranges[j].y, line.range.y);
			if (end <= start) {
				continue;
			}
			for (const Vector2 &segment : TS->shaped_text_get_selection(line_rid, start, end)) {
				const float x = p_offset.x + line.bounds.position.x + segment.x;
				const float width = segment.y - segment.x;
				const float baseline = p_offset.y + line.baseline;
				if (fragment.background.a > 0) {
					rendering->canvas_item_add_rect(p_canvas_item, Rect2(x, p_offset.y + line.bounds.position.y, width, line.bounds.size.y), fragment.background);
				}
				if (fragment.decorations & DECORATION_UNDERLINE) {
					rendering->canvas_item_add_rect(p_canvas_item, Rect2(x, baseline + underline - thickness / 2, width, thickness), fragment.color);
				}
				if (fragment.decorations & DECORATION_STRIKETHROUGH) {
					rendering->canvas_item_add_rect(p_canvas_item, Rect2(x, baseline + (descent - ascent) / 2 - thickness / 2, width, thickness), fragment.color);
				}
			}
		}
		_draw_glyphs(p_canvas_item, line_rid, p_offset + Point2(line.bounds.position.x, line.baseline));
	}
}
