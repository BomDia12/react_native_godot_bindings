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

// Yoga uses separate setters for points, percentages, and auto values.
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

// React Native puts the edge before "Width", unlike the other edge properties.
void apply_border_width(const Dictionary &p_style, YGNodeRef p_node) {
	struct BorderEdge {
		const char *key;
		YGEdge edge;
	};

	static const BorderEdge EDGES[] = {
		{ "borderWidth", YGEdgeAll },
		{ "borderTopWidth", YGEdgeTop },
		{ "borderBottomWidth", YGEdgeBottom },
		{ "borderLeftWidth", YGEdgeLeft },
		{ "borderRightWidth", YGEdgeRight },
	};

	for (const BorderEdge &entry : EDGES) {
		if (is_number(p_style.get(entry.key, Variant()))) {
			YGNodeStyleSetBorder(p_node, entry.edge, float(p_style[entry.key]));
		}
	}
}

YGWrap parse_flex_wrap(const String &p_value) {
	if (p_value == "wrap") {
		return YGWrapWrap;
	}
	if (p_value == "wrap-reverse") {
		return YGWrapWrapReverse;
	}
	return YGWrapNoWrap;
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
	return YGFlexDirectionColumn;
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

void apply_style(YGNodeRef p_node, const Dictionary &p_style) {
	if (p_style.has("flexDirection")) {
		YGNodeStyleSetFlexDirection(p_node, parse_flex_direction(p_style["flexDirection"]));
	}
	if (p_style.has("justifyContent")) {
		YGNodeStyleSetJustifyContent(p_node, parse_justify(p_style["justifyContent"]));
	}
	if (p_style.has("alignItems")) {
		YGNodeStyleSetAlignItems(p_node, parse_align(p_style["alignItems"], YGAlignStretch));
	}
	if (p_style.has("alignSelf")) {
		YGNodeStyleSetAlignSelf(p_node, parse_align(p_style["alignSelf"], YGAlignAuto));
	}
	if (p_style.has("alignContent")) {
		YGNodeStyleSetAlignContent(p_node, parse_align(p_style["alignContent"], YGAlignFlexStart));
	}
	if (p_style.has("flexWrap")) {
		YGNodeStyleSetFlexWrap(p_node, parse_flex_wrap(p_style["flexWrap"]));
	}

	if (is_number(p_style.get("flex", Variant()))) {
		YGNodeStyleSetFlex(p_node, float(p_style["flex"]));
	}
	if (is_number(p_style.get("flexGrow", Variant()))) {
		YGNodeStyleSetFlexGrow(p_node, float(p_style["flexGrow"]));
	}
	if (is_number(p_style.get("flexShrink", Variant()))) {
		YGNodeStyleSetFlexShrink(p_node, float(p_style["flexShrink"]));
	}
	if (p_style.has("flexBasis")) {
		apply_dimension(p_style["flexBasis"], p_node, YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent, YGNodeStyleSetFlexBasisAuto);
	}
	if (is_number(p_style.get("aspectRatio", Variant()))) {
		YGNodeStyleSetAspectRatio(p_node, float(p_style["aspectRatio"]));
	}

	if (p_style.has("width")) {
		apply_dimension(p_style["width"], p_node, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
	}
	if (p_style.has("height")) {
		apply_dimension(p_style["height"], p_node, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
	}
	if (p_style.has("minWidth")) {
		apply_dimension(p_style["minWidth"], p_node, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent, nullptr);
	}
	if (p_style.has("minHeight")) {
		apply_dimension(p_style["minHeight"], p_node, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent, nullptr);
	}
	if (p_style.has("maxWidth")) {
		apply_dimension(p_style["maxWidth"], p_node, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent, nullptr);
	}
	if (p_style.has("maxHeight")) {
		apply_dimension(p_style["maxHeight"], p_node, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent, nullptr);
	}

	apply_edge(p_style, "margin", p_node, YGNodeStyleSetMargin, YGNodeStyleSetMarginPercent);
	apply_edge(p_style, "padding", p_node, YGNodeStyleSetPadding, YGNodeStyleSetPaddingPercent);
	apply_border_width(p_style, p_node);

	if (is_number(p_style.get("gap", Variant()))) {
		YGNodeStyleSetGap(p_node, YGGutterAll, float(p_style["gap"]));
	}
	if (is_number(p_style.get("rowGap", Variant()))) {
		YGNodeStyleSetGap(p_node, YGGutterRow, float(p_style["rowGap"]));
	}
	if (is_number(p_style.get("columnGap", Variant()))) {
		YGNodeStyleSetGap(p_node, YGGutterColumn, float(p_style["columnGap"]));
	}

	if (p_style.has("display")) {
		const String display = p_style["display"];
		YGNodeStyleSetDisplay(p_node, display == "none" ? YGDisplayNone : YGDisplayFlex);
	}

	if (p_style.has("position")) {
		const String position = p_style["position"];
		YGNodeStyleSetPositionType(p_node, position == "absolute" ? YGPositionTypeAbsolute : YGPositionTypeRelative);
	}

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
		if (is_number(p_style.get(offset.key, Variant()))) {
			YGNodeStyleSetPosition(p_node, offset.edge, float(p_style[offset.key]));
		}
	}
}

float font_size_of(const Dictionary &p_props) {
	if (is_number(p_props.get("fontSize", Variant()))) {
		return float(p_props["fontSize"]);
	}
	return ThemeDB::get_singleton()->get_fallback_font_size();
}

// Yoga needs a measure function because text has no Yoga children.
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

// Godot Control positions and these layout rectangles are parent-relative.
void write_layout(YGNodeRef p_yoga_node) {
	RNShadowNode *shadow = static_cast<RNShadowNode *>(YGNodeGetContext(p_yoga_node));

	const Point2 origin(YGNodeLayoutGetLeft(p_yoga_node), YGNodeLayoutGetTop(p_yoga_node));
	const Size2 size(YGNodeLayoutGetWidth(p_yoga_node), YGNodeLayoutGetHeight(p_yoga_node));

	if (shadow) {
		shadow->layout = Rect2(origin, size);
	}

	for (size_t i = 0; i < YGNodeGetChildCount(p_yoga_node); ++i) {
		write_layout(YGNodeGetChild(p_yoga_node, i));
	}
}

} //namespace

void RNLayout::calculate(const Ref<RNShadowNode> &p_root, const Size2 &p_available) {
	if (p_root.is_null()) {
		return;
	}

	YGNodeRef yoga_root = build_yoga_tree(p_root);
	YGNodeCalculateLayout(yoga_root, float(p_available.width), float(p_available.height), YGDirectionLTR);
	write_layout(yoga_root);
	YGNodeFreeRecursive(yoga_root);
}
