#include "GodotTextInputProps.h"

#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

inline void fromRawValue(const PropsParserContext &, const RawValue &value, GodotTextInputSelection &result) {
	const auto map = static_cast<std::unordered_map<std::string, RawValue>>(value);
	const auto start = map.find("start");
	const auto end = map.find("end");
	result.start = start != map.end() && start->second.hasType<int>() ? static_cast<int>(start->second) : 0;
	result.end = end != map.end() && end->second.hasType<int>() ? static_cast<int>(end->second) : result.start;
}

GodotTextInputProps::GodotTextInputProps(const PropsParserContext &context, const GodotTextInputProps &sourceProps, const RawProps &rawProps) :
		BaseTextInputProps(context, sourceProps, rawProps),
		secureTextEntry(convertRawProp(context, rawProps, "secureTextEntry", sourceProps.secureTextEntry, false)),
		selection(convertRawProp(context, rawProps, "selection", sourceProps.selection, std::optional<GodotTextInputSelection>{})) {
}

void GodotTextInputProps::setProp(const PropsParserContext &context, RawPropsPropNameHash hash, const char *propName, const RawValue &value) {
	BaseTextInputProps::setProp(context, hash, propName, value);

	static auto defaults = GodotTextInputProps{};

	switch (hash) {
		RAW_SET_PROP_SWITCH_CASE_BASIC(secureTextEntry);
		RAW_SET_PROP_SWITCH_CASE_BASIC(selection);
	}
}

} // namespace facebook::react
