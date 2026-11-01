#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/ShadowNodeTraits.h>

namespace facebook::react::HostPlatformViewTraitsInitializer {

inline bool formsStackingContext(const ViewProps &) {
	return false;
}

// These Views need a Control of their own to receive focus, right clicks, hover and drags, or to paint a Godot resource.
inline bool formsView(const ViewProps &props) {
	return props.focusable || !props.godotContextMenu.empty() || !props.godotThemeType.empty() || !props.godotStyleBox.empty() || !props.godotMaterial.empty() || !props.tooltip.empty() || !props.godotDragData.isNull() || !props.godotDropTypes.isNull();
}

inline bool isKeyboardFocusable(const ViewProps &props) {
	return props.focusable;
}

} // namespace facebook::react::HostPlatformViewTraitsInitializer
