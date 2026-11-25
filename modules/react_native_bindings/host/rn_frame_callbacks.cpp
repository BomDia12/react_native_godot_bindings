#include "rn_frame_callbacks.h"

#include <react/timing/primitives.h>

#include <vector>

using namespace facebook;

void RNFrameCallbacks::install(jsi::Runtime &p_runtime) {
	auto shared = std::make_shared<Queue>();
	queue = shared;
	jsi::Object global = p_runtime.global();
	global.setProperty(p_runtime, "requestAnimationFrame", jsi::Function::createFromHostFunction(p_runtime, jsi::PropNameID::forAscii(p_runtime, "requestAnimationFrame"), 1, [shared](jsi::Runtime &p_rt, const jsi::Value &, const jsi::Value *p_args, size_t p_count) {
		if (p_count == 0 || !p_args[0].isObject() || !p_args[0].getObject(p_rt).isFunction(p_rt)) {
			throw jsi::JSError(p_rt, "The first argument to requestAnimationFrame must be a function.");
		}
		const uint64_t id = ++shared->last_id;
		shared->waiting.emplace(id, p_args[0].getObject(p_rt).getFunction(p_rt));
		return jsi::Value(double(id));
	}));
	global.setProperty(p_runtime, "cancelAnimationFrame", jsi::Function::createFromHostFunction(p_runtime, jsi::PropNameID::forAscii(p_runtime, "cancelAnimationFrame"), 1, [shared](jsi::Runtime &, const jsi::Value &, const jsi::Value *p_args, size_t p_count) {
		if (p_count > 0 && p_args[0].isNumber()) {
			const uint64_t id = uint64_t(p_args[0].asNumber());
			shared->waiting.erase(id);
			shared->running.erase(id);
		}
		return jsi::Value::undefined();
	}));
}

void RNFrameCallbacks::process_frame(uint64_t p_now_usec, const RNFrameClock::Display &p_display) {
	if (!clock.tick(p_now_usec, p_display)) {
		return;
	}
	const double timestamp = react::HighResTimeStamp::now().toDOMHighResTimeStamp();
	// A busy JS thread skips frames instead of running a burst of them later.
	if (frame_pending.exchange(true)) {
		return;
	}
	run_on_js([this, timestamp](jsi::Runtime &p_runtime) {
		frame_pending = false;
		_run(p_runtime, timestamp);
	});
}

// Callbacks registered while the frame runs wait for the next one. A throwing
// callback doesn't stop the others; its error is rethrown in a task of its own,
// which reports it as a timer's error is.
void RNFrameCallbacks::_run(jsi::Runtime &p_runtime, double p_timestamp) {
	const std::shared_ptr<Queue> locked = queue.lock();
	if (!locked) {
		return;
	}
	locked->running = std::move(locked->waiting);
	locked->waiting.clear();
	std::vector<jsi::JSError> errors;
	while (!locked->running.empty()) {
		jsi::Function callback = std::move(locked->running.begin()->second);
		locked->running.erase(locked->running.begin());
		try {
			callback.call(p_runtime, p_timestamp);
		} catch (jsi::JSError &error) {
			errors.push_back(std::move(error));
		}
	}
	for (jsi::JSError &error : errors) {
		run_on_js([error = std::move(error)](jsi::Runtime &) { throw error; });
	}
}
