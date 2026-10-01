#pragma once

#include "../components/rn_builtin_descriptors.h"

#include "tests/test_macros.h"

namespace TestRNDescriptors {

TEST_CASE("[ReactNativeBindings][Descriptors] definitions are exact, duplicate-safe, and frozen") {
	RNHostDescriptorRegistry registry;
	RNError error;
	REQUIRE(rn_register_builtin_descriptors(registry, error));
	CHECK(registry.has("RCTRootView"));
	CHECK(registry.has("RCTView"));
	CHECK(registry.has("RCTText"));
	CHECK(registry.has("RCTRawText"));
	CHECK_FALSE(rn_register_builtin_descriptors(registry, error));
	CHECK(error.code == RNErrorCode::DUPLICATE_REGISTRATION);
	registry.freeze();
	CHECK(registry.is_frozen());
}

} // namespace TestRNDescriptors
