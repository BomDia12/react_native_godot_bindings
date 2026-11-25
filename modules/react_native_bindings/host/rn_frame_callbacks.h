#pragma once

#include "rn_frame_clock.h"

#include <jsi/jsi.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>

// requestAnimationFrame and cancelAnimationFrame, run on the JS thread once per
// display frame of the Godot frame clock. Upstream's TimerManager runs them as
// setTimeout(0), as often as the JS thread loops.
class RNFrameCallbacks {
public:
	using RunOnJS = std::function<void(std::function<void(facebook::jsi::Runtime &)>)>;

private:
	// The globals own the callbacks, so they go away with the runtime.
	struct Queue {
		uint64_t last_id = 0;
		std::map<uint64_t, facebook::jsi::Function> waiting;
		std::map<uint64_t, facebook::jsi::Function> running;
	};

	RunOnJS run_on_js;
	RNFrameClock clock;
	std::atomic<bool> frame_pending = false;
	// Used on the JS thread.
	std::weak_ptr<Queue> queue;

	void _run(facebook::jsi::Runtime &p_runtime, double p_timestamp);

public:
	explicit RNFrameCallbacks(RunOnJS p_run_on_js) :
			run_on_js(std::move(p_run_on_js)) {}

	// Replaces upstream's globals; runs on the JS thread.
	void install(facebook::jsi::Runtime &p_runtime);
	// Runs on the main thread once per Godot frame.
	void process_frame(uint64_t p_now_usec, const RNFrameClock::Display &p_display);
};
