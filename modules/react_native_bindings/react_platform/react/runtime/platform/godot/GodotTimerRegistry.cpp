#include "GodotTimerRegistry.h"

#include <react/renderer/runtimescheduler/RuntimeScheduler.h>
#include <react/runtime/TimerManager.h>

#include <chrono>

namespace facebook::react {

GodotTimerRegistry::GodotTimerRegistry(std::shared_ptr<const ContextContainer> contextContainer) :
		contextContainer_(std::move(contextContainer)) {
}

void GodotTimerRegistry::createTimer(uint32_t timerId, double delayMs) {
	add(timerId, delayMs, false);
}

void GodotTimerRegistry::createRecurringTimer(uint32_t timerId, double delayMs) {
	add(timerId, delayMs, true);
}

void GodotTimerRegistry::deleteTimer(uint32_t timerId) {
	std::lock_guard lock(mutex_);
	timers_.erase(timerId);
}

void GodotTimerRegistry::setTimerManager(std::weak_ptr<TimerManager> timerManager) {
	timerManager_ = std::move(timerManager);
}

void GodotTimerRegistry::quit() {
	thread_.quit();
	std::lock_guard lock(mutex_);
	timers_.clear();
}

void GodotTimerRegistry::add(uint32_t timerId, double delayMs, bool recurring) {
	{
		std::lock_guard lock(mutex_);
		timers_[timerId] = Timer{ .delayMs = delayMs, .recurring = recurring };
	}
	start(timerId, delayMs);
}

void GodotTimerRegistry::start(uint32_t timerId, double delayMs) {
	thread_.runAsync([this, timerId] { fire(timerId); }, std::chrono::milliseconds(static_cast<int32_t>(delayMs)));
}

void GodotTimerRegistry::fire(uint32_t timerId) {
	bool recurring = false;
	bool call = false;
	double delayMs = 0;
	{
		std::lock_guard lock(mutex_);
		auto timer = timers_.find(timerId);
		if (timer == timers_.end()) {
			return;
		}
		recurring = timer->second.recurring;
		delayMs = timer->second.delayMs;
		call = !timer->second.queued;
		if (!recurring) {
			timers_.erase(timer);
		} else {
			timer->second.queued = true;
		}
	}
	if (call) {
		if (auto timerManager = timerManager_.lock()) {
			timerManager->callTimer(timerId);
		}
		if (!recurring) {
			return;
		}
		// Work scheduled later at the same priority expires later, so the flag
		// clears once the tick that callTimer just queued has run.
		if (auto scheduler = runtimeScheduler()) {
			scheduler->scheduleWork([this, timerId](jsi::Runtime &) { unqueue(timerId); });
		} else {
			unqueue(timerId);
		}
	}
	start(timerId, delayMs);
}

void GodotTimerRegistry::unqueue(uint32_t timerId) {
	std::lock_guard lock(mutex_);
	if (auto timer = timers_.find(timerId); timer != timers_.end()) {
		timer->second.queued = false;
	}
}

std::shared_ptr<RuntimeScheduler> GodotTimerRegistry::runtimeScheduler() const {
	auto scheduler = contextContainer_->find<std::weak_ptr<RuntimeScheduler>>(RuntimeSchedulerKey);
	return scheduler ? scheduler->lock() : nullptr;
}

} // namespace facebook::react
