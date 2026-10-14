#pragma once

#include <folly/dynamic.h>
#include <react/renderer/components/view/BaseViewProps.h>

namespace facebook::react {

class HostPlatformViewProps : public BaseViewProps {
public:
	HostPlatformViewProps() = default;
	HostPlatformViewProps(const PropsParserContext &context, const HostPlatformViewProps &sourceProps, const RawProps &rawProps, const std::function<bool(const std::string &)> &filterObjectKeys = nullptr);

	void setProp(const PropsParserContext &context, RawPropsPropNameHash hash, const char *propName, const RawValue &value);

	bool focusable{ false };
	folly::dynamic godotContextMenu{ folly::dynamic::array() };
};

} // namespace facebook::react
