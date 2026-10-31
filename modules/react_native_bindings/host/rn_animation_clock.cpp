#include "rn_animation_clock.h"

#include <mutex>

namespace {
using Clock = std::chrono::steady_clock;

std::mutex mutex;
bool paused = false;
Clock::time_point paused_at;
Clock::duration paused_for{ 0 };
} // namespace

Clock::time_point rn_animation_clock_now() {
	std::lock_guard lock(mutex);
	return (paused ? paused_at : Clock::now()) - paused_for;
}

void rn_set_animation_clock_paused(bool p_paused) {
	std::lock_guard lock(mutex);
	if (paused == p_paused) {
		return;
	}
	paused = p_paused;
	if (paused) {
		paused_at = Clock::now();
	} else {
		paused_for += Clock::now() - paused_at;
	}
}
