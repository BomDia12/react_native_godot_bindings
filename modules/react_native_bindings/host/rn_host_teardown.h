#pragma once

#include <react/renderer/core/ReactPrimitives.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace facebook::hermes {
class IHermes;
} // namespace facebook::hermes

namespace facebook::react {
class GodotTimerRegistry;
class ReactHost;
} // namespace facebook::react

class RNAsyncStorageState;

// One upstream runtime, with the handles that stop it while JavaScript may
// still be running. The JS thread publishes the Hermes pointer once the
// runtime is initialized.
struct RNHostRuntime {
	std::unique_ptr<facebook::react::ReactHost> react_host;
	facebook::react::GodotTimerRegistry *timers = nullptr;
	std::atomic<facebook::hermes::IHermes *> hermes = nullptr;

	RNHostRuntime();
	~RNHostRuntime();
};

// Stops a runtime's surfaces and destroys it on a thread of its own. The main
// thread waits for it only when the game quits, and then for a bounded time.
// JavaScript that keeps the runtime busy for more than a second is interrupted.
class RNHostTeardown {
	RNHostRuntime &runtime;
	std::vector<facebook::react::SurfaceId> surfaces;
	std::shared_ptr<RNAsyncStorageState> async_storage;
	std::promise<void> finishing;
	std::future<void> finished = finishing.get_future();
	std::thread thread;

	void _run();
	void _wait_until_idle();

public:
	RNHostTeardown(RNHostRuntime &p_runtime, std::vector<facebook::react::SurfaceId> p_surfaces, std::shared_ptr<RNAsyncStorageState> p_async_storage);
	~RNHostTeardown();

	bool is_finished() const { return finished.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
	bool join(std::chrono::milliseconds p_timeout);
};
