#include "rn_main_thread_queue.h"

RNMainThreadQueue::RNMainThreadQueue() :
		main_thread(std::this_thread::get_id()) {
}

void RNMainThreadQueue::post(std::function<void()> p_work) {
	std::lock_guard lock(mutex);
	if (closed) {
		return;
	}
	tasks.push_back({ std::move(p_work), nullptr });
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
	}
	return completed.get();
}

void RNMainThreadQueue::drain() {
	std::deque<Task> pending;
	{
		std::lock_guard lock(mutex);
		pending.swap(tasks);
	}
	for (Task &task : pending) {
		const bool open = !is_closed();
		if (open) {
			task.work();
		}
		if (task.completion) {
			task.completion->set_value(open);
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
}

bool RNMainThreadQueue::is_closed() const {
	std::lock_guard lock(mutex);
	return closed;
}
