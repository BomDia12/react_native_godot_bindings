#include "rn_feature_flags.h"

#include <react/featureflags/ReactNativeFeatureFlags.h>

void rn_install_feature_flags() {
	facebook::react::ReactNativeFeatureFlags::override(std::make_unique<RNFeatureFlags>());
}
