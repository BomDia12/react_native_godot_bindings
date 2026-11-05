#include "HostPlatformViewProps.h"

#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

HostPlatformViewProps::HostPlatformViewProps(const PropsParserContext &context, const HostPlatformViewProps &sourceProps, const RawProps &rawProps, const std::function<bool(const std::string &)> &filterObjectKeys) :
		BaseViewProps(context, sourceProps, rawProps, filterObjectKeys),
		focusable(convertRawProp(context, rawProps, "focusable", sourceProps.focusable, {})),
		godotContextMenu(convertRawProp(context, rawProps, "godotContextMenu", sourceProps.godotContextMenu, folly::dynamic::array())),
		godotThemeType(convertRawProp(context, rawProps, "godotThemeType", sourceProps.godotThemeType, {})),
		godotStyleBox(convertRawProp(context, rawProps, "godotStyleBox", sourceProps.godotStyleBox, {})),
		godotMaterial(convertRawProp(context, rawProps, "godotMaterial", sourceProps.godotMaterial, {})),
		godotShaderParameters(convertRawProp(context, rawProps, "godotShaderParameters", sourceProps.godotShaderParameters, nullptr)),
		tooltip(convertRawProp(context, rawProps, "tooltip", sourceProps.tooltip, {})),
		godotDragData(convertRawProp(context, rawProps, "godotDragData", sourceProps.godotDragData, nullptr)),
		godotDragPreview(convertRawProp(context, rawProps, "godotDragPreview", sourceProps.godotDragPreview, nullptr)),
		godotDropTypes(convertRawProp(context, rawProps, "godotDropTypes", sourceProps.godotDropTypes, nullptr)),
		hasTVPreferredFocus(convertRawProp(context, rawProps, "hasTVPreferredFocus", sourceProps.hasTVPreferredFocus, {})),
		nextFocusDown(convertRawProp(context, rawProps, "nextFocusDown", sourceProps.nextFocusDown, {})),
		nextFocusForward(convertRawProp(context, rawProps, "nextFocusForward", sourceProps.nextFocusForward, {})),
		nextFocusLeft(convertRawProp(context, rawProps, "nextFocusLeft", sourceProps.nextFocusLeft, {})),
		nextFocusRight(convertRawProp(context, rawProps, "nextFocusRight", sourceProps.nextFocusRight, {})),
		nextFocusUp(convertRawProp(context, rawProps, "nextFocusUp", sourceProps.nextFocusUp, {})) {
}

void HostPlatformViewProps::setProp(const PropsParserContext &context, RawPropsPropNameHash hash, const char *propName, const RawValue &value) {
	BaseViewProps::setProp(context, hash, propName, value);

	static auto defaults = HostPlatformViewProps{};

	switch (hash) {
		RAW_SET_PROP_SWITCH_CASE_BASIC(focusable);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotContextMenu);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotThemeType);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotStyleBox);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotMaterial);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotShaderParameters);
		RAW_SET_PROP_SWITCH_CASE_BASIC(tooltip);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotDragData);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotDragPreview);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotDropTypes);
		RAW_SET_PROP_SWITCH_CASE_BASIC(hasTVPreferredFocus);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusDown);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusForward);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusLeft);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusRight);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusUp);
	}
}

} // namespace facebook::react
