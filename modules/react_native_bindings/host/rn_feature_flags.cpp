#include "rn_feature_flags.h"

#include <react/featureflags/ReactNativeFeatureFlags.h>

#include <mutex>

void rn_install_feature_flags() {
	static std::once_flag installed;
	std::call_once(installed, [] {
		facebook::react::ReactNativeFeatureFlags::override(std::make_unique<RNFeatureFlags>());
	});
}
