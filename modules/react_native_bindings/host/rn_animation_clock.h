#pragma once

#include <chrono>

// The clock upstream's native Animated driver reads. It stands still while the
// runtime is suspended, so native-driver animations resume where they stopped.
std::chrono::steady_clock::time_point rn_animation_clock_now();
void rn_set_animation_clock_paused(bool p_paused);
