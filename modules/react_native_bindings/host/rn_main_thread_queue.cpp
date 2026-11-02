#include "rn_main_thread_queue.h"

#include <algorithm>
#include <chrono>

namespace {

// A sync call blocks JS until the main thread answers, so a loop of them would
// otherwise get one answer per frame. The main thread briefly waits for the next
// one, which is the only time it waits, and never for longer than the budget.
constexpr std::chrono::microseconds NEXT_SYNC_CALL_WAIT(200);
constexpr std::chrono::milliseconds SYNC_CALLS_BUDGET(4);

} // namespace

RNMainThreadQueue::RNMainThreadQueue() :
		main_thread(std::this_thread::get_id()) {
}

void RNMainThreadQueue::post(std::function<void()> p_work) {
	std::lock_guard lock(mutex);
	if (closed) {
		return;
	}
	tasks.push_back({ std::move(p_work), nullptr });
	task_added.notify_one();
}

bool RNMainThreadQueue::run_sync(std::function<void()> p_work) {
	if (is_main_thread()) {
		if (is_closed()) {
			return false;
		}
		p_work();
		return true;
	}
	auto completion = std::make_shared<std::promise<bool>>();
	std::future<bool> completed = completion->get_future();
	{
		std::lock_guard lock(mutex);
		if (closed) {
			return false;
		}
		tasks.push_back({ std::move(p_work), completion });
		task_added.notify_one();
	}
	return completed.get();
}

void RNMainThreadQueue::drain() {
	const auto deadline = std::chrono::steady_clock::now() + SYNC_CALLS_BUDGET;
	while (true) {
		std::deque<Task> pending;
		{
			std::lock_guard lock(mutex);
			pending.swap(tasks);
		}
		bool answered_sync_call = false;
		for (Task &task : pending) {
			const bool open = !is_closed();
			if (open) {
				task.work();
			}
			if (task.completion) {
				task.completion->set_value(open);
				answered_sync_call = true;
			}
		}
		if (!answered_sync_call) {
			return;
		}
		const auto wait_until = std::min(std::chrono::steady_clock::now() + NEXT_SYNC_CALL_WAIT, deadline);
		std::unique_lock lock(mutex);
		if (!task_added.wait_until(lock, wait_until, [this] { return closed || !tasks.empty(); }) || closed) {
			return;
		}
	}
}

void RNMainThreadQueue::close() {
	std::lock_guard lock(mutex);
	closed = true;
	for (Task &task : tasks) {
		if (task.completion) {
			task.completion->set_value(false);
		}
	}
	tasks.clear();
	task_added.notify_all();
}

bool RNMainThreadQueue::is_closed() const {
	std::lock_guard lock(mutex);
	return closed;
}
