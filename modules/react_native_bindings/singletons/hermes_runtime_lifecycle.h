#pragma once

#include <cstdint>

namespace facebook::jsi {
class Runtime;
}

class HermesRuntimeLifecycle {
public:
	virtual ~HermesRuntimeLifecycle() = default;
	virtual void before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) = 0;
};
