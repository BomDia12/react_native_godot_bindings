#pragma once

#include "../runtime/react_native_runtime_coordinator.h"

#include <jsi/jsi.h>

#include <memory>

class NativeDOM : public facebook::jsi::HostObject {
	std::weak_ptr<RNRuntimeCoordinatorState> state;

public:
	explicit NativeDOM(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) : state(p_state) {}

	facebook::jsi::Value get(facebook::jsi::Runtime &rt, const facebook::jsi::PropNameID &name) override;
	std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime &rt) override;
};
