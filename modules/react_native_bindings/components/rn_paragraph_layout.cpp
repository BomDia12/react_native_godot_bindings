#include "rn_paragraph_layout.h"

#include "servers/rendering/rendering_server.h"

#include <functional>

namespace {

constexpr float UNBOUNDED_WIDTH = 1.0e7f;
const String ELLIPSIS = String::chr(0x2026);

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

// The pieces of p_fragments between p_from and p_to, in characters of their joined text.
void slice(const Vector<RNParagraphLayout::Fragment> &p_fragments, int p_from, int p_to, Vector<RNParagraphLayout::Fragment> &r_result) {
	int offset = 0;
	for (const RNParagraphLayout::Fragment &fragment : p_fragments) {
		const int length = fragment.text.length();
		const int start = MAX(p_from, offset);
		const int end = MIN(p_to, offset + length);
		if (end > start) {
			RNParagraphLayout::Fragment piece = fragment;
			piece.text = fragment.text.substr(start - offset, end - start);
			r_result.push_back(piece);
		}
		offset += length;
	}
}

const RNParagraphLayout::Fragment &fragment_containing(const Vector<RNParagraphLayout::Fragment> &p_fragments, int p_character) {
	int offset = 0;
	for (const RNParagraphLayout::Fragment &fragment : p_fragments) {
		offset += fragment.text.length();
		if (p_character < offset) {
			return fragment;
		}
	}
	return p_fragments[p_fragments.size() - 1];
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
	truncated = false;
	elided = Vector2i(-1, -1);
}

int RNParagraphLayout::fragment_at(int p_character) const {
	for (int i = 0; i < fragment_ranges.size(); ++i) {
		if (p_character >= fragment_ranges[i].x && p_character < fragment_ranges[i].y) {
			return i;
		}
	}
	return fragments.size() - 1;
}

void RNParagraphLayout::shape(const Vector<Fragment> &p_fragments, const Attributes &p_attributes) {
	if (p_attributes.max_lines == 1 && p_attributes.ellipsis != ELLIPSIS_TAIL && p_attributes.width > 0 && _elide(p_fragments, p_attributes)) {
		return;
	}
	_shape(p_fragments, p_attributes);
}

// Head and middle ellipses keep the longest suffix, or prefix and suffix, that
// fits on one line with the ellipsis, cut at grapheme boundaries.
bool RNParagraphLayout::_elide(const Vector<Fragment> &p_fragments, const Attributes &p_attributes) {
	Attributes single = p_attributes;
	single.ellipsis = ELLIPSIS_TAIL;
	single.overrun = TextServer::OVERRUN_NO_TRIMMING;
	_shape(p_fragments, single);
	if (!truncated || fragments.is_empty()) {
		return true;
	}
	String text;
	for (const Fragment &fragment : p_fragments) {
		text += fragment.text;
	}
	const int length = text.length();
	Vector<int> boundaries = { 0 };
	for (int end : TS->string_get_character_breaks(text)) {
		boundaries.push_back(end);
	}
	auto boundary_at_or_below = [&boundaries](int p_index) {
		int result = 0;
		for (int boundary : boundaries) {
			if (boundary <= p_index) {
				result = boundary;
			}
		}
		return result;
	};
	auto boundary_at_or_above = [&boundaries, length](int p_index) {
		for (int boundary : boundaries) {
			if (boundary >= p_index) {
				return boundary;
			}
		}
		return length;
	};
	auto cut = [&](int p_kept) {
		const int prefix = p_attributes.ellipsis == ELLIPSIS_HEAD ? 0 : boundary_at_or_below((p_kept + 1) / 2);
		const int suffix = boundary_at_or_above(length - (p_kept - prefix));
		return Vector2i(prefix, MAX(prefix, suffix));
	};
	auto candidate = [&](const Vector2i &p_cut) {
		Vector<Fragment> result;
		slice(p_fragments, 0, p_cut.x, result);
		Fragment ellipsis = fragment_containing(p_fragments, MIN(p_cut.x, length - 1));
		ellipsis.text = ELLIPSIS;
		result.push_back(ellipsis);
		slice(p_fragments, p_cut.y, length, result);
		return result;
	};
	auto fits = [&](const Vector2i &p_cut) {
		Attributes unbounded = single;
		unbounded.width = -1;
		_shape(candidate(p_cut), unbounded);
		return lines.size() == 1 && size.x <= p_attributes.width;
	};
	int low = 0;
	int high = length - 1;
	while (low < high) {
		const int middle = (low + high + 1) / 2;
		if (fits(cut(middle))) {
			low = middle;
		} else {
			high = middle - 1;
		}
	}
	const Vector2i kept = cut(low);
	_shape(candidate(kept), single);
	truncated = true;
	elided = kept;
	return true;
}

int RNParagraphLayout::_to_layout(int p_index) const {
	if (elided.x < 0 || p_index < elided.x) {
		return p_index;
	}
	return p_index < elided.y ? elided.x : p_index - elided.y + elided.x + 1;
}

int RNParagraphLayout::_to_original(int p_index) const {
	if (elided.x < 0 || p_index <= elided.x) {
		return p_index;
	}
	return p_index - elided.x - 1 + elided.y;
}

void RNParagraphLayout::_shape(const Vector<Fragment> &p_fragments, const Attributes &p_attributes) {
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
	const bool limited = p_attributes.max_lines > 0 && line_count > p_attributes.max_lines;
	truncated = limited;
	if (limited) {
		line_count = p_attributes.max_lines;
	}
	auto add_line = [this](const Vector2i &p_range, RID p_line, float p_line_height, float p_ascent, float p_descent) {
		line_rids.push_back(p_line);
		const float height = p_line_height > 0 ? p_line_height : p_ascent + p_descent;
		Line record;
		record.range = p_range;
		record.bounds = Rect2(0, size.y, TS->shaped_text_get_width(p_line), height);
		record.baseline = size.y + p_ascent + (height - p_ascent - p_descent) / 2;
		lines.push_back(record);
		size = Size2(MAX(size.x, record.bounds.size.x), size.y + height);
	};
	for (int i = 0; i < line_count; ++i) {
		const Vector2i range(breaks[i * 2], breaks[i * 2 + 1]);
		const RID line = TS->shaped_text_substr(paragraph, range.x, range.y - range.x);
		const bool last = i == line_count - 1;
		if (wraps && p_attributes.alignment == HORIZONTAL_ALIGNMENT_FILL && !last) {
			TS->shaped_text_fit_to_width(line, p_attributes.width, TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA);
		}
		const bool overflows = wraps && TS->shaped_text_get_width(line) > p_attributes.width;
		truncated = truncated || (last && overflows);
		if (last && (limited || overflows)) {
			BitField<TextServer::TextOverrunFlag> flags = overrun_flags(p_attributes.overrun);
			if (limited && p_attributes.overrun != TextServer::OVERRUN_NO_TRIMMING) {
				flags.set_flag(TextServer::OVERRUN_ENFORCE_ELLIPSIS);
			}
			TS->shaped_text_overrun_trim_to_width(line, wraps ? p_attributes.width : TS->shaped_text_get_width(line), flags);
		}
		float line_height = 0;
		for (int j = 0; j < fragments.size(); ++j) {
			if (fragment_ranges[j].x < range.y && fragment_ranges[j].y > range.x) {
				line_height = MAX(line_height, fragments[j].line_height);
			}
		}
		add_line(range, line, line_height, TS->shaped_text_get_ascent(line), TS->shaped_text_get_descent(line));
	}

	// A trailing newline starts an empty last line, as in an editor.
	const Fragment &last_fragment = fragments[fragments.size() - 1];
	const bool room = p_attributes.max_lines <= 0 || lines.size() < p_attributes.max_lines;
	if (!limited && room && !lines.is_empty() && lines[lines.size() - 1].range.x < offset && last_fragment.text.ends_with("\n") && last_fragment.font.is_valid()) {
		add_line(Vector2i(offset, offset), TS->create_shaped_text(), last_fragment.line_height, last_fragment.font->get_ascent(last_fragment.font_size), last_fragment.font->get_descent(last_fragment.font_size));
	}

	const float container = wraps ? p_attributes.width : size.x;
	for (Line &line : lines) {
		const float free_space = container - line.bounds.size.x;
		if (p_attributes.alignment == HORIZONTAL_ALIGNMENT_CENTER) {
			line.bounds.position.x = Math::floor(free_space / 2);
		} else if (p_attributes.alignment == HORIZONTAL_ALIGNMENT_RIGHT) {
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
	const Line &line = lines[index];
	if (line.range.x == line.range.y) {
		return _to_original(line.range.x);
	}
	return _to_original(TS->shaped_text_hit_test_position(line_rids[index], p_point.x - line.bounds.position.x));
}

int RNParagraphLayout::character_at(const Point2 &p_point) const {
	for (int i = 0; i < lines.size(); ++i) {
		const Line &line = lines[i];
		if (line.bounds.has_point(p_point) && line.range.x < line.range.y) {
			const int glyph = TS->shaped_text_hit_test_grapheme(line_rids[i], p_point.x - line.bounds.position.x);
			return glyph < 0 ? -1 : _to_original(TS->shaped_text_get_glyphs(line_rids[i])[glyph].start);
		}
	}
	return -1;
}

Vector<Rect2> RNParagraphLayout::get_range_rects(int p_from, int p_to) const {
	const int from = _to_layout(p_from);
	const int to = elided.x >= 0 && p_to > elided.x && p_to <= elided.y ? elided.x + 1 : _to_layout(p_to);
	Vector<Rect2> result;
	for (int i = 0; i < lines.size(); ++i) {
		const Line &line = lines[i];
		const int start = MAX(from, line.range.x);
		const int end = MIN(to, line.range.y);
		if (end <= start) {
			continue;
		}
		for (const Vector2 &segment : TS->shaped_text_get_selection(line_rids[i], start, end)) {
			result.push_back(Rect2(line.bounds.position.x + segment.x, line.bounds.position.y, segment.y - segment.x, line.bounds.size.y));
		}
	}
	return result;
}

void RNParagraphLayout::_draw_glyphs(RID p_canvas_item, RID p_line, const Point2 &p_origin, bool p_shadows) const {
	const bool rtl = TS->shaped_text_get_direction(p_line) == TextServer::DIRECTION_RTL;
	const int trim = TS->shaped_text_get_trim_pos(p_line);
	const int ellipsis_position = TS->shaped_text_get_ellipsis_pos(p_line);
	const Glyph *ellipsis = TS->shaped_text_get_ellipsis_glyphs(p_line);
	const int ellipsis_count = TS->shaped_text_get_ellipsis_glyph_count(p_line);
	const Glyph *glyphs = TS->shaped_text_get_glyphs(p_line);
	const int count = TS->shaped_text_get_glyph_count(p_line);
	const Fragment *ellipsis_fragment = fragments.is_empty() ? nullptr : &fragments[fragments.size() - 1];
	Vector2 offset = p_origin;

	auto draw_glyph = [&](const Glyph &p_glyph, const Fragment &p_fragment) {
		if (!p_glyph.font_rid.is_valid()) {
			return;
		}
		const Vector2 position = offset + Vector2(p_glyph.x_off, p_glyph.y_off);
		if (!p_shadows) {
			TS->font_draw_glyph(p_glyph.font_rid, p_canvas_item, p_glyph.font_size, position, p_glyph.index, p_fragment.color);
		} else if (p_fragment.shadow_color.a > 0) {
			const Vector2 shadow = position + p_fragment.shadow_offset;
			if (p_fragment.shadow_radius > 0) {
				TS->font_draw_glyph_outline(p_glyph.font_rid, p_canvas_item, p_glyph.font_size, int(Math::ceil(p_fragment.shadow_radius)), shadow, p_glyph.index, p_fragment.shadow_color);
			}
			TS->font_draw_glyph(p_glyph.font_rid, p_canvas_item, p_glyph.font_size, shadow, p_glyph.index, p_fragment.shadow_color);
		}
	};
	auto draw_ellipsis = [&]() {
		for (int i = 0; i < ellipsis_count && ellipsis_fragment; ++i) {
			const Glyph &glyph = ellipsis[rtl ? ellipsis_count - 1 - i : i];
			for (int repeat = 0; repeat < glyph.repeat; ++repeat) {
				draw_glyph(glyph, *ellipsis_fragment);
				offset.x += glyph.advance;
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
		const Fragment &fragment = fragments[fragment_at(glyphs[i].start)];
		ellipsis_fragment = &fragment;
		for (int repeat = 0; repeat < glyphs[i].repeat; ++repeat) {
			draw_glyph(glyphs[i], fragment);
			offset.x += glyphs[i].advance;
		}
	}
	if (!rtl && ellipsis_position >= 0) {
		draw_ellipsis();
	}
}

void RNParagraphLayout::_draw_decoration(RID p_canvas_item, const Fragment &p_fragment, float p_x, float p_y, float p_width, float p_thickness) const {
	RenderingServer *rendering = RenderingServer::get_singleton();
	const Color color = p_fragment.decoration_color;
	switch (p_fragment.decoration_style) {
		case DECORATION_DOUBLE: {
			rendering->canvas_item_add_rect(p_canvas_item, Rect2(p_x, p_y - p_thickness, p_width, p_thickness), color);
			rendering->canvas_item_add_rect(p_canvas_item, Rect2(p_x, p_y + p_thickness, p_width, p_thickness), color);
		} break;
		case DECORATION_DOTTED:
		case DECORATION_DASHED: {
			const float segment = p_fragment.decoration_style == DECORATION_DOTTED ? p_thickness : p_thickness * 3;
			for (float x = 0; x < p_width; x += segment * 2) {
				rendering->canvas_item_add_rect(p_canvas_item, Rect2(p_x + x, p_y - p_thickness / 2, MIN(segment, p_width - x), p_thickness), color);
			}
		} break;
		default: {
			rendering->canvas_item_add_rect(p_canvas_item, Rect2(p_x, p_y - p_thickness / 2, p_width, p_thickness), color);
		} break;
	}
}

// Each line draws backgrounds, then shadows, decorations and glyphs, so
// decorations sit over shadows and under glyphs.
void RNParagraphLayout::draw(RID p_canvas_item, const Point2 &p_offset) const {
	RenderingServer *rendering = RenderingServer::get_singleton();
	for (int i = 0; i < lines.size(); ++i) {
		const Line &line = lines[i];
		const RID line_rid = line_rids[i];
		const float ascent = TS->shaped_text_get_ascent(line_rid);
		const float descent = TS->shaped_text_get_descent(line_rid);
		const float underline = TS->shaped_text_get_underline_position(line_rid);
		const float thickness = MAX(1.0, TS->shaped_text_get_underline_thickness(line_rid));
		const Point2 origin = p_offset + Point2(line.bounds.position.x, line.baseline);
		auto for_each_segment = [&](const std::function<void(const Fragment &, float, float)> &p_callback) {
			for (int j = 0; j < fragments.size(); ++j) {
				const int start = MAX(fragment_ranges[j].x, line.range.x);
				const int end = MIN(fragment_ranges[j].y, line.range.y);
				if (end <= start) {
					continue;
				}
				for (const Vector2 &segment : TS->shaped_text_get_selection(line_rid, start, end)) {
					p_callback(fragments[j], p_offset.x + line.bounds.position.x + segment.x, segment.y - segment.x);
				}
			}
		};
		for_each_segment([&](const Fragment &p_fragment, float p_x, float p_width) {
			if (p_fragment.background.a > 0) {
				rendering->canvas_item_add_rect(p_canvas_item, Rect2(p_x, p_offset.y + line.bounds.position.y, p_width, line.bounds.size.y), p_fragment.background);
			}
		});
		_draw_glyphs(p_canvas_item, line_rid, origin, true);
		for_each_segment([&](const Fragment &p_fragment, float p_x, float p_width) {
			if (p_fragment.decorations & DECORATION_UNDERLINE) {
				_draw_decoration(p_canvas_item, p_fragment, p_x, origin.y + underline, p_width, thickness);
			}
			if (p_fragment.decorations & DECORATION_STRIKETHROUGH) {
				_draw_decoration(p_canvas_item, p_fragment, p_x, origin.y + (descent - ascent) / 2, p_width, thickness);
			}
		});
		_draw_glyphs(p_canvas_item, line_rid, origin, false);
	}
}
