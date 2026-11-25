#include "rn_frame_callbacks.h"

#include <react/timing/primitives.h>

#include <string>
#include <utility>
#include <vector>

using namespace facebook;

void RNFrameCallbacks::install(jsi::Runtime &p_runtime) {
	auto shared = std::make_shared<Queue>();
	queue = shared;
	jsi::Object global = p_runtime.global();
	global.setProperty(p_runtime, "requestAnimationFrame", jsi::Function::createFromHostFunction(p_runtime, jsi::PropNameID::forAscii(p_runtime, "requestAnimationFrame"), 1, [this, shared](jsi::Runtime &p_rt, const jsi::Value &, const jsi::Value *p_args, size_t p_count) {
		if (p_count == 0 || !p_args[0].isObject() || !p_args[0].getObject(p_rt).isFunction(p_rt)) {
			throw jsi::JSError(p_rt, "The first argument to requestAnimationFrame must be a function.");
		}
		const uint64_t id = ++shared->last_id;
		shared->waiting.emplace(id, p_args[0].getObject(p_rt).getFunction(p_rt));
		requested = true;
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

// Frames tick whether or not anything waits, so a first request after idling
// lands on the clock's grid, but the JS thread only wakes while callbacks wait.
void RNFrameCallbacks::process_frame(uint64_t p_now_usec, const RNFrameClock::Display &p_display) {
	if (!clock.tick(p_now_usec, p_display) || !requested) {
		return;
	}
	// A busy JS thread skips frames instead of running a burst of them later.
	if (frame_pending.exchange(true)) {
		return;
	}
	++posted;
	run_on_js([this](jsi::Runtime &p_runtime) {
		frame_pending = false;
		_run(p_runtime, react::HighResTimeStamp::now().toDOMHighResTimeStamp());
	});
}

// Callbacks registered while the frame runs wait for the next one. A throwing
// callback doesn't stop the others; its error is thrown again from a task of
// its own, which reports it as a timer's error is. The task keeps only the
// error's text, so no JavaScript value outlives a runtime that stops first.
void RNFrameCallbacks::_run(jsi::Runtime &p_runtime, double p_timestamp) {
	const std::shared_ptr<Queue> locked = queue.lock();
	if (!locked) {
		return;
	}
	locked->running = std::move(locked->waiting);
	locked->waiting.clear();
	std::vector<std::pair<std::string, std::string>> errors;
	while (!locked->running.empty()) {
		jsi::Function callback = std::move(locked->running.begin()->second);
		locked->running.erase(locked->running.begin());
		try {
			callback.call(p_runtime, p_timestamp);
		} catch (const jsi::JSError &error) {
			errors.emplace_back(error.getMessage(), error.getStack());
		} catch (const std::exception &error) {
			errors.emplace_back(error.what(), std::string());
		}
	}
	requested = !locked->waiting.empty();
	for (auto &[message, stack] : errors) {
		run_on_js([message, stack](jsi::Runtime &p_rt) {
			jsi::Object error = p_rt.global().getPropertyAsFunction(p_rt, "Error").callAsConstructor(p_rt, message).asObject(p_rt);
			error.setProperty(p_rt, "stack", stack);
			throw jsi::JSError(p_rt, jsi::Value(p_rt, error));
		});
	}
}
