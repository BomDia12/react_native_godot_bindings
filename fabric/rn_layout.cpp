#include "rn_layout.h"

#include "core/math/math_defs.h"
#include "scene/resources/font.h"
#include "scene/theme/theme_db.h"

#include <yoga/Yoga.h>

#include <cmath>

namespace {

bool is_number(const Variant &p_value) {
	return p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT;
}

// Style values arrive as numbers ("width: 100"), percent strings ("width: '50%'")
// or "auto". Each Yoga setter comes in three flavours, so dispatch on the shape.
void apply_dimension(const Variant &p_value, YGNodeRef p_node,
		void (*p_set_points)(YGNodeRef, float),
		void (*p_set_percent)(YGNodeRef, float),
		void (*p_set_auto)(YGNodeRef)) {
	if (is_number(p_value)) {
		p_set_points(p_node, float(p_value));
		return;
	}

	if (p_value.get_type() == Variant::STRING) {
		const String text = p_value;
		if (text == "auto") {
			if (p_set_auto) {
				p_set_auto(p_node);
			}
		} else if (text.ends_with("%") && p_set_percent) {
			p_set_percent(p_node, text.trim_suffix("%").to_float());
		}
	}
}

void apply_edge(const Dictionary &p_style, const String &p_prefix, YGNodeRef p_node,
		void (*p_set_points)(YGNodeRef, YGEdge, float),
		void (*p_set_percent)(YGNodeRef, YGEdge, float)) {
	struct EdgeName {
		const char *suffix;
		YGEdge edge;
	};

	static const EdgeName EDGES[] = {
		{ "", YGEdgeAll },
		{ "Top", YGEdgeTop },
		{ "Bottom", YGEdgeBottom },
		{ "Left", YGEdgeLeft },
		{ "Right", YGEdgeRight },
		{ "Horizontal", YGEdgeHorizontal },
		{ "Vertical", YGEdgeVertical },
	};

	for (const EdgeName &entry : EDGES) {
		const String key = p_prefix + String(entry.suffix);
		if (!p_style.has(key)) {
			continue;
		}

		const Variant value = p_style[key];
		if (is_number(value)) {
			p_set_points(p_node, entry.edge, float(value));
		} else if (value.get_type() == Variant::STRING) {
			const String text = value;
			if (text.ends_with("%") && p_set_percent) {
				p_set_percent(p_node, entry.edge, text.trim_suffix("%").to_float());
			}
		}
	}
}

YGFlexDirection parse_flex_direction(const String &p_value) {
	if (p_value == "row") {
		return YGFlexDirectionRow;
	}
	if (p_value == "row-reverse") {
		return YGFlexDirectionRowReverse;
	}
	if (p_value == "column-reverse") {
		return YGFlexDirectionColumnReverse;
	}
	return YGFlexDirectionColumn; // RN's default, unlike the web's.
}

YGJustify parse_justify(const String &p_value) {
	if (p_value == "flex-end") {
		return YGJustifyFlexEnd;
	}
	if (p_value == "center") {
		return YGJustifyCenter;
	}
	if (p_value == "space-between") {
		return YGJustifySpaceBetween;
	}
	if (p_value == "space-around") {
		return YGJustifySpaceAround;
	}
	if (p_value == "space-evenly") {
		return YGJustifySpaceEvenly;
	}
	return YGJustifyFlexStart;
}

YGAlign parse_align(const String &p_value, YGAlign p_default) {
	if (p_value == "flex-start") {
		return YGAlignFlexStart;
	}
	if (p_value == "flex-end") {
		return YGAlignFlexEnd;
	}
	if (p_value == "center") {
		return YGAlignCenter;
	}
	if (p_value == "stretch") {
		return YGAlignStretch;
	}
	if (p_value == "baseline") {
		return YGAlignBaseline;
	}
	if (p_value == "auto") {
		return YGAlignAuto;
	}
	return p_default;
}

void apply_style(YGNodeRef p_node, const Dictionary &p_props) {
	const Variant style_variant = p_props.get("style", Variant());
	if (style_variant.get_type() != Variant::DICTIONARY) {
		return;
	}

	const Dictionary style = style_variant;

	if (style.has("flexDirection")) {
		YGNodeStyleSetFlexDirection(p_node, parse_flex_direction(style["flexDirection"]));
	}
	if (style.has("justifyContent")) {
		YGNodeStyleSetJustifyContent(p_node, parse_justify(style["justifyContent"]));
	}
	if (style.has("alignItems")) {
		YGNodeStyleSetAlignItems(p_node, parse_align(style["alignItems"], YGAlignStretch));
	}
	if (style.has("alignSelf")) {
		YGNodeStyleSetAlignSelf(p_node, parse_align(style["alignSelf"], YGAlignAuto));
	}

	if (is_number(style.get("flex", Variant()))) {
		YGNodeStyleSetFlex(p_node, float(style["flex"]));
	}
	if (is_number(style.get("flexGrow", Variant()))) {
		YGNodeStyleSetFlexGrow(p_node, float(style["flexGrow"]));
	}
	if (is_number(style.get("flexShrink", Variant()))) {
		YGNodeStyleSetFlexShrink(p_node, float(style["flexShrink"]));
	}

	if (style.has("width")) {
		apply_dimension(style["width"], p_node, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
	}
	if (style.has("height")) {
		apply_dimension(style["height"], p_node, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
	}
	if (style.has("minWidth")) {
		apply_dimension(style["minWidth"], p_node, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent, nullptr);
	}
	if (style.has("minHeight")) {
		apply_dimension(style["minHeight"], p_node, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent, nullptr);
	}
	if (style.has("maxWidth")) {
		apply_dimension(style["maxWidth"], p_node, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent, nullptr);
	}
	if (style.has("maxHeight")) {
		apply_dimension(style["maxHeight"], p_node, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent, nullptr);
	}

	apply_edge(style, "margin", p_node, YGNodeStyleSetMargin, YGNodeStyleSetMarginPercent);
	apply_edge(style, "padding", p_node, YGNodeStyleSetPadding, YGNodeStyleSetPaddingPercent);

	if (style.has("position")) {
		const String position = style["position"];
		YGNodeStyleSetPositionType(p_node, position == "absolute" ? YGPositionTypeAbsolute : YGPositionTypeRelative);
	}

	// top/left/right/bottom, which Yoga models as position offsets.
	struct Offset {
		const char *key;
		YGEdge edge;
	};
	static const Offset OFFSETS[] = {
		{ "top", YGEdgeTop },
		{ "bottom", YGEdgeBottom },
		{ "left", YGEdgeLeft },
		{ "right", YGEdgeRight },
	};
	for (const Offset &offset : OFFSETS) {
		if (is_number(style.get(offset.key, Variant()))) {
			YGNodeStyleSetPosition(p_node, offset.edge, float(style[offset.key]));
		}
	}
}

float font_size_of(const Dictionary &p_props) {
	const Variant style_variant = p_props.get("style", Variant());
	if (style_variant.get_type() == Variant::DICTIONARY) {
		const Dictionary style = style_variant;
		if (is_number(style.get("fontSize", Variant()))) {
			return float(style["fontSize"]);
		}
	}
	return ThemeDB::get_singleton()->get_fallback_font_size();
}

// Text is a leaf as far as flexbox is concerned: Yoga cannot know how wide a string
// is, so it asks us. Without this an RCTText measures 0x0 and nothing is visible.
YGSize measure_text(YGNodeConstRef p_node, float p_width, YGMeasureMode p_width_mode, float p_height, YGMeasureMode p_height_mode) {
	(void)p_height;
	(void)p_height_mode;

	const RNShadowNode *shadow = static_cast<const RNShadowNode *>(YGNodeGetContext(p_node));
	if (!shadow) {
		return YGSize{ 0.0f, 0.0f };
	}

	const Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
	if (font.is_null()) {
		return YGSize{ 0.0f, 0.0f };
	}

	const String text = shadow->collect_text();
	const float font_size = font_size_of(shadow->props);

	// Only wrap when Yoga actually constrains the width.
	const float wrap_width = (p_width_mode == YGMeasureModeUndefined || !std::isfinite(p_width)) ? -1.0f : p_width;
	const Size2 size = font->get_multiline_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, wrap_width, font_size);

	float width = float(size.width);
	if (p_width_mode == YGMeasureModeExactly) {
		width = p_width;
	} else if (p_width_mode == YGMeasureModeAtMost) {
		width = MIN(width, p_width);
	}

	return YGSize{ width, float(size.height) };
}

YGNodeRef build_yoga_tree(const Ref<RNShadowNode> &p_node) {
	YGNodeRef yoga_node = YGNodeNew();
	YGNodeSetContext(yoga_node, const_cast<RNShadowNode *>(p_node.ptr()));
	apply_style(yoga_node, p_node->props);

	if (p_node->view_name == "RCTText") {
		// Its RCTRawText/RCTVirtualText children are text, not boxes: they are folded
		// into the measure function rather than laid out.
		YGNodeSetMeasureFunc(yoga_node, measure_text);
		return yoga_node;
	}

	size_t index = 0;
	for (const Ref<RNShadowNode> &child : p_node->children) {
		if (child.is_null() || child->view_name == "RCTRawText") {
			continue;
		}
		YGNodeInsertChild(yoga_node, build_yoga_tree(child), index++);
	}

	return yoga_node;
}

void write_layout(YGNodeRef p_yoga_node, const Point2 &p_parent_origin) {
	RNShadowNode *shadow = static_cast<RNShadowNode *>(YGNodeGetContext(p_yoga_node));

	const Point2 origin = p_parent_origin + Point2(YGNodeLayoutGetLeft(p_yoga_node), YGNodeLayoutGetTop(p_yoga_node));
	const Size2 size = Size2(YGNodeLayoutGetWidth(p_yoga_node), YGNodeLayoutGetHeight(p_yoga_node));

	if (shadow) {
		shadow->layout = Rect2(origin, size);
	}

	for (size_t i = 0; i < YGNodeGetChildCount(p_yoga_node); ++i) {
		write_layout(YGNodeGetChild(p_yoga_node, i), origin);
	}
}

} //namespace

void RNLayout::calculate(const Ref<RNShadowNode> &p_root, const Size2 &p_available) {
	if (p_root.is_null()) {
		return;
	}

	YGNodeRef yoga_root = build_yoga_tree(p_root);
	YGNodeCalculateLayout(yoga_root, float(p_available.width), float(p_available.height), YGDirectionLTR);
	write_layout(yoga_root, Point2());
	YGNodeFreeRecursive(yoga_root);
}
