#include "rn_host_teardown.h"

#include <jsi/hermes-interfaces.h>
#include <react/runtime/PlatformTimerRegistry.h>
#include <react/runtime/ReactHost.h>

#include <future>
#include <mutex>

using namespace facebook;
using namespace facebook::react;

namespace {

struct IdleMarker {
	std::mutex mutex;
	bool started = false;
	bool interrupted = false;
	std::promise<void> idle;
};

} // namespace

RNHostRuntime::RNHostRuntime() = default;
RNHostRuntime::~RNHostRuntime() = default;

RNHostTeardown::RNHostTeardown(RNHostRuntime &p_runtime, std::vector<SurfaceId> p_surfaces) :
		runtime(p_runtime), surfaces(std::move(p_surfaces)) {
	thread = std::thread([this] { _run(); });
}

RNHostTeardown::~RNHostTeardown() {
	thread.join();
}

void RNHostTeardown::_run() {
	if (runtime.timers) {
		runtime.timers->quit();
	}
	_wait_until_idle();
	for (SurfaceId surface : surfaces) {
		runtime.react_host->stopSurface(surface);
		_wait_until_idle();
	}
	runtime.react_host.reset();
	finished = true;
}

// The marker runs at LowPriority, after the unmount and cleanup work that React
// schedules at higher priorities, and expires within 10 s even if more keeps
// coming.
void RNHostTeardown::_wait_until_idle() {
	auto marker = std::make_shared<IdleMarker>();
	std::future<void> idle = marker->idle.get_future();
	runtime.react_host->runOnRuntimeScheduler([marker](jsi::Runtime &p_runtime) {
		bool interrupted = false;
		{
			std::lock_guard lock(marker->mutex);
			marker->started = true;
			interrupted = marker->interrupted;
		}
		if (interrupted) {
			// An interrupt that arrived after the busy JavaScript returned stays
			// armed until JavaScript next checks for it. Consume it here, so it
			// cannot cut short the work that follows.
			try {
				p_runtime.evaluateJavaScript(std::make_shared<jsi::StringBuffer>("for (let i = 0; i < 1; i++) {}"), "godot://teardown");
			} catch (const jsi::JSIException &) {
			}
		}
		marker->idle.set_value();
	},
			SchedulerPriority::LowPriority);
	while (idle.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
		std::lock_guard lock(marker->mutex);
		facebook::hermes::IHermes *engine = runtime.hermes;
		if (!marker->started && engine) {
			marker->interrupted = true;
			engine->asyncTriggerTimeout();
		}
	}
}
