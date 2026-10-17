#pragma once

#include <react/runtime/PlatformTimerRegistry.h>
#include <react/threading/TaskDispatchThread.h>
#include <react/utils/ContextContainer.h>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace facebook::react {

class RuntimeScheduler;

// Fires JS timers like PlatformTimerRegistryImpl, but keeps at most one tick of
// each interval waiting for the JS thread. While JavaScript is busy, interval
// ticks are skipped, as browsers do, instead of piling up behind it.
class GodotTimerRegistry : public PlatformTimerRegistry {
	struct Timer {
		double delayMs = 0;
		bool recurring = false;
		bool queued = false;
	};

	TaskDispatchThread thread_{ "GodotTimerRegistry" };
	std::shared_ptr<const ContextContainer> contextContainer_;
	std::weak_ptr<TimerManager> timerManager_;
	std::mutex mutex_;
	std::unordered_map<uint32_t, Timer> timers_;

	void add(uint32_t timerId, double delayMs, bool recurring);
	void start(uint32_t timerId, double delayMs);
	void fire(uint32_t timerId);
	void unqueue(uint32_t timerId);
	std::shared_ptr<RuntimeScheduler> runtimeScheduler() const;

public:
	explicit GodotTimerRegistry(std::shared_ptr<const ContextContainer> contextContainer);

	void createTimer(uint32_t timerId, double delayMs) override;
	void createRecurringTimer(uint32_t timerId, double delayMs) override;
	void deleteTimer(uint32_t timerId) override;
	void setTimerManager(std::weak_ptr<TimerManager> timerManager) override;
	void quit() override;
};

} // namespace facebook::react
