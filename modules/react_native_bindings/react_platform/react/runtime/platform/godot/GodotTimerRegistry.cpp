#include "GodotTimerRegistry.h"

#include <react/renderer/runtimescheduler/RuntimeScheduler.h>
#include <react/runtime/TimerManager.h>

#include <algorithm>
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
	std::lock_guard lock(mutex_);
	Timer &timer = timers_[timerId] = Timer{ .delayMs = delayMs, .recurring = recurring };
	schedule(timerId, timer, delayMs);
}

void GodotTimerRegistry::schedule(uint32_t timerId, Timer &timer, double delayMs) {
	if (paused_) {
		timer.tick = 0;
		timer.remainingMs = delayMs;
		return;
	}
	const auto delay = std::chrono::milliseconds(static_cast<int32_t>(delayMs));
	timer.tick = nextTick_++;
	timer.due = Clock::now() + delay;
	thread_.runAsync([this, timerId, tick = timer.tick] { fire(timerId, tick); }, delay);
}

void GodotTimerRegistry::setPaused(bool paused) {
	std::lock_guard lock(mutex_);
	if (paused_ == paused) {
		return;
	}
	paused_ = paused;
	for (auto &[timerId, timer] : timers_) {
		if (paused) {
			timer.remainingMs = std::max(0.0, std::chrono::duration<double, std::milli>(timer.due - Clock::now()).count());
			timer.tick = 0;
		} else {
			schedule(timerId, timer, timer.remainingMs);
		}
	}
}

void GodotTimerRegistry::fire(uint32_t timerId, uint64_t tick) {
	bool recurring = false;
	bool call = false;
	{
		std::lock_guard lock(mutex_);
		auto timer = timers_.find(timerId);
		if (timer == timers_.end() || timer->second.tick != tick) {
			return;
		}
		recurring = timer->second.recurring;
		call = !timer->second.queued;
		if (!recurring) {
			timers_.erase(timer);
		} else {
			timer->second.queued = true;
			schedule(timerId, timer->second, timer->second.delayMs);
		}
	}
	if (!call) {
		return;
	}
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
