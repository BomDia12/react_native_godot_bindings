#pragma once

#include <react/renderer/components/textinput/BaseTextInputProps.h>

#include <optional>

namespace facebook::react {

struct GodotTextInputSelection {
	int start{ 0 };
	int end{ 0 };

	bool operator==(const GodotTextInputSelection &) const = default;
};

class GodotTextInputProps final : public BaseTextInputProps {
public:
	GodotTextInputProps() = default;
	GodotTextInputProps(const PropsParserContext &context, const GodotTextInputProps &sourceProps, const RawProps &rawProps);

	void setProp(const PropsParserContext &context, RawPropsPropNameHash hash, const char *propName, const RawValue &value);

	bool secureTextEntry{ false };
	std::optional<GodotTextInputSelection> selection{};
};

} // namespace facebook::react
