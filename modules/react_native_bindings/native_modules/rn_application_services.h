#pragma once

#include "rn_native_module_registry.h"

struct RNRuntimeCoordinatorState;
bool rn_register_application_services(RNNativeModuleRegistry &p_registry, const std::shared_ptr<RNRuntimeCoordinatorState> &p_state, RNError &r_error);
