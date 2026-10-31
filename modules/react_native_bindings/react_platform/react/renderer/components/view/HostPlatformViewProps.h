#pragma once

#include <folly/dynamic.h>
#include <react/renderer/components/view/BaseViewProps.h>

#include <optional>

namespace facebook::react {

class HostPlatformViewProps : public BaseViewProps {
public:
	HostPlatformViewProps() = default;
	HostPlatformViewProps(const PropsParserContext &context, const HostPlatformViewProps &sourceProps, const RawProps &rawProps, const std::function<bool(const std::string &)> &filterObjectKeys = nullptr);

	void setProp(const PropsParserContext &context, RawPropsPropNameHash hash, const char *propName, const RawValue &value);

	bool focusable{ false };
	folly::dynamic godotContextMenu{ folly::dynamic::array() };
	// Upstream's TV focus props: the React tags of the views focus moves to.
	bool hasTVPreferredFocus{ false };
	std::optional<int> nextFocusDown{};
	std::optional<int> nextFocusForward{};
	std::optional<int> nextFocusLeft{};
	std::optional<int> nextFocusRight{};
	std::optional<int> nextFocusUp{};
};

} // namespace facebook::react
