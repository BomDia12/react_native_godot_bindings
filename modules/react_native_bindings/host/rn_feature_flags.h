#pragma once

#include <react/featureflags/ReactNativeFeatureFlagsDefaults.h>

// Every flag keeps its upstream default except the C++ native Animated driver,
// which Godot uses instead of a platform Animated implementation.
class RNFeatureFlags : public facebook::react::ReactNativeFeatureFlagsDefaults {
public:
	bool cxxNativeAnimatedEnabled() override { return true; }
};

void rn_install_feature_flags();
