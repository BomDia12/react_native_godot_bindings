#pragma once

#include <react/featureflags/ReactNativeFeatureFlagsDefaults.h>

// Upstream defaults, with the flags Godot's platform needs: the bridgeless
// architecture that ReactHost runs (its Hermes runtime only enables microtasks
// with it), the C++ native Animated driver, W3C pointer events for hover,
// because Godot reports mouse hover only through pointer events, and imperative
// focus, because Godot views take keyboard focus.
class RNFeatureFlags : public facebook::react::ReactNativeFeatureFlagsDefaults {
public:
	bool enableBridgelessArchitecture() override { return true; }
	bool cxxNativeAnimatedEnabled() override { return true; }
	bool shouldPressibilityUseW3CPointerEventsForHover() override { return true; }
	bool enableImperativeFocus() override { return true; }
};

void rn_install_feature_flags();
