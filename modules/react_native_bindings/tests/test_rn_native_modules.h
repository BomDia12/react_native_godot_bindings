#pragma once

#include "../native_modules/rn_native_module.h"

#include "tests/test_macros.h"

namespace TestRNNativeModules {

class EchoModule : public RNNativeModule {
public:
	RNModuleResult invoke_sync(const StringName &, const Array &p_arguments, const RNCallContext &) override {
		return RNModuleResult::success(p_arguments.is_empty() ? Variant() : p_arguments[0]);
	}
};

TEST_CASE("[ReactNativeBindings][NativeModules] sync modules receive copied native arguments") {
	EchoModule module;
	Array arguments;
	arguments.push_back(String("value"));
	RNModuleResult result = module.invoke_sync("echo", arguments, RNCallContext());
	CHECK_FALSE(result.error.is_set());
	CHECK(String(result.value) == "value");
}

} // namespace TestRNNativeModules
