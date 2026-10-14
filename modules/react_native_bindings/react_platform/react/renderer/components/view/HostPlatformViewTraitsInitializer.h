#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/ShadowNodeTraits.h>

namespace facebook::react::HostPlatformViewTraitsInitializer {

inline bool formsStackingContext(const ViewProps &) {
	return false;
}

// A focusable View or one with a context menu needs its own Control to receive focus and right clicks.
inline bool formsView(const ViewProps &props) {
	return props.focusable || !props.godotContextMenu.empty();
}

inline bool isKeyboardFocusable(const ViewProps &props) {
	return props.focusable;
}

} // namespace facebook::react::HostPlatformViewTraitsInitializer
