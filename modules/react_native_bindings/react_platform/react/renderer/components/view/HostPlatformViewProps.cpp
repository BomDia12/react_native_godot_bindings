#include "HostPlatformViewProps.h"

#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

HostPlatformViewProps::HostPlatformViewProps(const PropsParserContext &context, const HostPlatformViewProps &sourceProps, const RawProps &rawProps, const std::function<bool(const std::string &)> &filterObjectKeys) :
		BaseViewProps(context, sourceProps, rawProps, filterObjectKeys),
		focusable(convertRawProp(context, rawProps, "focusable", sourceProps.focusable, {})),
		godotContextMenu(convertRawProp(context, rawProps, "godotContextMenu", sourceProps.godotContextMenu, folly::dynamic::array())) {
}

void HostPlatformViewProps::setProp(const PropsParserContext &context, RawPropsPropNameHash hash, const char *propName, const RawValue &value) {
	BaseViewProps::setProp(context, hash, propName, value);

	static auto defaults = HostPlatformViewProps{};

	switch (hash) {
		RAW_SET_PROP_SWITCH_CASE_BASIC(focusable);
		RAW_SET_PROP_SWITCH_CASE_BASIC(godotContextMenu);
	}
}

} // namespace facebook::react
