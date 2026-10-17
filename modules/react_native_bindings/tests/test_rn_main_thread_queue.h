#pragma once

#include "../host/rn_main_thread_queue.h"

#include "tests/test_macros.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace TestRNMainThreadQueue {

TEST_CASE("[ReactNativeBindings][MainThreadQueue] posted work runs in order when the main thread drains") {
	RNMainThreadQueue queue;
	Vector<int> order;
	queue.post([&order]() { order.push_back(1); });
	queue.post([&order]() { order.push_back(2); });
	CHECK(order.is_empty());
	queue.drain();
	CHECK(order == Vector<int>({ 1, 2 }));
}

TEST_CASE("[ReactNativeBindings][MainThreadQueue] sync calls block their thread until the main thread runs them") {
	RNMainThreadQueue queue;
	std::atomic<bool> finished = false;
	std::atomic<bool> succeeded = false;
	int value = 0;
	std::thread worker([&]() {
		succeeded = queue.run_sync([&value]() { value = 42; });
		finished = true;
	});
	while (!finished) {
		queue.drain();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	worker.join();
	CHECK(succeeded);
	CHECK(value == 42);
	CHECK(queue.run_sync([&value]() { value = 7; }));
	CHECK(value == 7);
}

TEST_CASE("[ReactNativeBindings][MainThreadQueue] close cancels blocked and new sync calls and discards posted work") {
	RNMainThreadQueue queue;
	std::atomic<bool> finished = false;
	std::atomic<bool> succeeded = true;
	bool ran = false;
	queue.post([&ran]() { ran = true; });
	std::thread worker([&]() {
		succeeded = queue.run_sync([&ran]() { ran = true; });
		finished = true;
	});
	while (!finished) {
		queue.close();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	worker.join();
	CHECK_FALSE(succeeded);
	queue.post([&ran]() { ran = true; });
	queue.drain();
	CHECK_FALSE(ran);
	std::thread late([&]() { succeeded = queue.run_sync([&ran]() { ran = true; }); });
	late.join();
	CHECK_FALSE(succeeded);
	CHECK_FALSE(queue.run_sync([&ran]() { ran = true; }));
	CHECK_FALSE(ran);
}

} // namespace TestRNMainThreadQueue
