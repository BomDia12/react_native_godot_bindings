#include "HostPlatformViewProps.h"

#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

HostPlatformViewProps::HostPlatformViewProps(const PropsParserContext &context, const HostPlatformViewProps &sourceProps, const RawProps &rawProps, const std::function<bool(const std::string &)> &filterObjectKeys) :
		BaseViewProps(context, sourceProps, rawProps, filterObjectKeys),
		focusable(convertRawProp(context, rawProps, "focusable", sourceProps.focusable, {})),
		godotContextMenu(convertRawProp(context, rawProps, "godotContextMenu", sourceProps.godotContextMenu, folly::dynamic::array())),
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
		RAW_SET_PROP_SWITCH_CASE_BASIC(hasTVPreferredFocus);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusDown);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusForward);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusLeft);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusRight);
		RAW_SET_PROP_SWITCH_CASE_BASIC(nextFocusUp);
	}
}

} // namespace facebook::react
