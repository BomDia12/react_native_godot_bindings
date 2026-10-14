#pragma once

#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

class RNMainThreadQueue {
	struct Task {
		std::function<void()> work;
		std::shared_ptr<std::promise<bool>> completion;
	};

	mutable std::mutex mutex;
	std::deque<Task> tasks;
	std::thread::id main_thread;
	bool shutting_down = false;

public:
	RNMainThreadQueue();

	bool is_main_thread() const { return std::this_thread::get_id() == main_thread; }
	void post(std::function<void()> p_work);
	bool run_sync(std::function<void()> p_work);
	void drain();

	void begin_shutdown();
	void end_shutdown();
	bool is_shutting_down() const;
};
