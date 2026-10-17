#pragma once

#include <react/renderer/core/ReactPrimitives.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace facebook::hermes {
class IHermes;
} // namespace facebook::hermes

namespace facebook::react {
class PlatformTimerRegistry;
class ReactHost;
} // namespace facebook::react

// One upstream runtime, with the handles that stop it while JavaScript may
// still be running. The JS thread publishes the Hermes pointer once the
// runtime is initialized.
struct RNHostRuntime {
	std::unique_ptr<facebook::react::ReactHost> react_host;
	facebook::react::PlatformTimerRegistry *timers = nullptr;
	std::atomic<facebook::hermes::IHermes *> hermes = nullptr;

	RNHostRuntime();
	~RNHostRuntime();
};

// Stops a runtime's surfaces and destroys it on a thread of its own, so the
// main thread never waits for JavaScript. JavaScript that keeps the runtime
// busy for more than a second is interrupted.
class RNHostTeardown {
	RNHostRuntime &runtime;
	std::vector<facebook::react::SurfaceId> surfaces;
	std::atomic<bool> finished = false;
	std::thread thread;

	void _run();
	void _wait_until_idle();

public:
	RNHostTeardown(RNHostRuntime &p_runtime, std::vector<facebook::react::SurfaceId> p_surfaces);
	~RNHostTeardown();

	bool is_finished() const { return finished; }
};
