#include "rn_runtime_scheduler.h"

#include "core/error/error_macros.h"
#include "core/os/os.h"
#include "core/string/ustring.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace facebook;

RNRuntimeScheduler::RNRuntimeScheduler(Clock p_clock) : clock(std::move(p_clock)) {
	if (!clock) {
		clock = [] { return double(OS::get_singleton()->get_ticks_usec()) / 1000.0; };
	}
	time_origin = clock();
}

double RNRuntimeScheduler::now() const {
	return std::max(0.0, clock() - time_origin);
}

void RNRuntimeScheduler::configure(size_t p_max_tasks, double p_idle_budget) {
	max_tasks = p_max_tasks;
	frame_remaining = max_tasks;
	idle_budget = p_idle_budget;
}

jsi::Value RNRuntimeScheduler::schedule(jsi::Runtime &p_runtime, Kind p_kind, const jsi::Value *p_arguments, size_t p_count) {
	if (!p_count || !p_arguments[0].isObject() || !p_arguments[0].getObject(p_runtime).isFunction(p_runtime)) {
		throw jsi::JSError(p_runtime, "Scheduled callback must be a function.");
	}
	if (tasks.size() >= 65536 || p_count > 4096) {
		throw jsi::JSError(p_runtime, "Scheduler retained-task or argument limit exceeded.");
	}
	if (next_id > 9007199254740991ULL) {
		throw jsi::JSError(p_runtime, "Scheduler callback IDs exhausted.");
	}
	double delay = 0;
	if (p_count > 1 && p_kind != Kind::FRAME) {
		jsi::Value value(p_runtime, p_arguments[1]);
		if (p_kind == Kind::IDLE) {
			value = value.isObject() ? value.getObject(p_runtime).getProperty(p_runtime, "timeout") : jsi::Value::undefined();
		}
		if (!value.isUndefined()) {
			const jsi::Value number = p_runtime.global().getPropertyAsFunction(p_runtime, "Number").call(p_runtime, value);
			delay = number.getNumber();
			if (!std::isfinite(delay) || delay < 0 || delay > double(INT32_MAX)) {
				delay = 0;
			}
			delay = std::floor(delay);
		}
	}
	Task task;
	task.origin = RNExecutionScope::current();
	task.kind = p_kind;
	task.due = p_kind == Kind::IDLE && delay <= 0 ? std::numeric_limits<double>::infinity() : now() + delay;
	task.interval = std::max(1.0, delay);
	task.callback = std::make_unique<jsi::Function>(p_arguments[0].getObject(p_runtime).getFunction(p_runtime));
	if (p_kind == Kind::TIMEOUT || p_kind == Kind::INTERVAL) {
		for (size_t index = 2; index < p_count; ++index) {
			task.arguments.emplace_back(p_runtime, p_arguments[index]);
		}
	}
	const uint64_t id = next_id++;
	tasks.emplace(id, std::move(task));
	if (p_kind == Kind::IDLE) {
		jsi::Object handle(p_runtime);
		handle.setProperty(p_runtime, "id", double(id));
		return handle;
	}
	return jsi::Value(double(id));
}

void RNRuntimeScheduler::drain(jsi::Runtime &p_runtime, bool p_idle, bool p_visual_frame, const std::function<void()> &p_checkpoint, double p_available_ms) {
	if (draining) {
		return;
	}
	draining = true;
	struct Guard {
		bool &flag;
		~Guard() { flag = false; }
	} guard{ draining };
	const double frame_now = now();
	const double available = std::max(0.0, std::min(idle_budget, p_available_ms));
	const double deadline = frame_now + available;
	std::vector<uint64_t> due;
	for (const auto &entry : tasks) {
		const Task &task = entry.second;
		if ((task.kind == Kind::IDLE) != p_idle || (task.kind == Kind::FRAME && !p_visual_frame)) {
			continue;
		}
		if (task.kind == Kind::FRAME || task.due <= frame_now || (p_idle && available > 0)) {
			due.push_back(entry.first);
		}
	}
	for (uint64_t id : due) {
		auto found = tasks.find(id);
		if (found == tasks.end() || frame_remaining == 0) {
			continue;
		}
		if (p_idle && now() >= deadline && found->second.due > now()) {
			continue;
		}
		Task task = std::move(found->second);
		tasks.erase(found);
		jsi::Function callback = jsi::Value(p_runtime, *task.callback).getObject(p_runtime).getFunction(p_runtime);
		if (task.kind == Kind::INTERVAL) {
			task.due += (std::floor(std::max(0.0, frame_now - task.due) / task.interval) + 1) * task.interval;
		}
		std::vector<jsi::Value> arguments;
		for (const auto &argument : task.arguments) {
			arguments.emplace_back(p_runtime, argument);
		}
		if (task.kind == Kind::FRAME) {
			arguments.emplace_back(frame_now);
		} else if (task.kind == Kind::IDLE) {
			jsi::Object value(p_runtime);
			value.setProperty(p_runtime, "didTimeout", task.due <= frame_now);
			std::weak_ptr<RNRuntimeScheduler> weak = weak_from_this();
			value.setProperty(p_runtime, "timeRemaining", jsi::Function::createFromHostFunction(p_runtime, jsi::PropNameID::forAscii(p_runtime, "timeRemaining"), 0, [weak, deadline](jsi::Runtime &, const jsi::Value &, const jsi::Value *, size_t) {
				auto scheduler = weak.lock();
				return jsi::Value(scheduler ? std::max(0.0, deadline - scheduler->now()) : 0.0);
			}));
			arguments.emplace_back(std::move(value));
		}
		if (task.kind == Kind::INTERVAL) {
			tasks.emplace(id, std::move(task));
		}
		--frame_remaining;
		try {
			RNExecutionScope scope(task.kind == Kind::INTERVAL ? tasks.at(id).origin : task.origin);
			callback.call(p_runtime, static_cast<const jsi::Value *>(arguments.data()), arguments.size());
		} catch (const jsi::JSIException &exception) {
			ERR_PRINT(String::utf8(exception.what()));
		}
		p_checkpoint();
	}
}

void RNRuntimeScheduler::process_frame_locked(jsi::Runtime &p_runtime, bool p_visual_frame, const std::function<void()> &p_checkpoint, size_t p_native_delivered) {
	frame_remaining = max_tasks - std::min(max_tasks, p_native_delivered);
	drain(p_runtime, false, p_visual_frame, p_checkpoint);
}

void RNRuntimeScheduler::process_idle_locked(jsi::Runtime &p_runtime, const std::function<void()> &p_checkpoint, double p_available_ms) {
	drain(p_runtime, true, false, p_checkpoint, p_available_ms);
	frame_remaining = max_tasks;
}

jsi::Value RNRuntimeScheduler::get(jsi::Runtime &p_runtime, const jsi::PropNameID &p_name) {
	const std::string name = p_name.utf8(p_runtime);
	if (name != "getOrigin" && name != "withOrigin" && name != "withoutOrigin" && name != "now" && name != "setTimeout" && name != "setInterval" && name != "clearTimeout" && name != "clearInterval" && name != "requestAnimationFrame" && name != "cancelAnimationFrame" && name != "requestIdleCallback" && name != "cancelIdleCallback") {
		return jsi::Value::undefined();
	}
	const std::weak_ptr<RNRuntimeScheduler> weak = weak_from_this();
	return jsi::Function::createFromHostFunction(p_runtime, jsi::PropNameID::forUtf8(p_runtime, name), 2,
			[weak, name](jsi::Runtime &rt, const jsi::Value &, const jsi::Value *args, size_t count) -> jsi::Value {
				auto scheduler = weak.lock();
				if (!scheduler) {
					throw jsi::JSError(rt, "Scheduler is closed.");
				}
				if (name == "withoutOrigin" || name == "withOrigin") {
					const size_t callback_index = name == "withOrigin" ? 1 : 0;
					if (count <= callback_index || !args[callback_index].isObject() || !args[callback_index].getObject(rt).isFunction(rt)) {
						throw jsi::JSError(rt, "Execution scope requires a callback.");
					}
					RNExecutionOrigin origin;
					if (name == "withOrigin" && !args[0].isNull()) {
						const jsi::Object value = args[0].getObject(rt);
						auto integer = [&](const char *key, double maximum) {
							const jsi::Value number = value.getProperty(rt, key);
							if (!number.isNumber() || !std::isfinite(number.getNumber()) || number.getNumber() < 1 || number.getNumber() > maximum || std::trunc(number.getNumber()) != number.getNumber()) {
								throw jsi::JSError(rt, "Execution origin requires positive safe integers.");
							}
							return number.getNumber();
						};
						origin = { uint64_t(integer("generation", 9007199254740991.0)), int(integer("rootTag", INT32_MAX)), uint64_t(integer("epoch", 9007199254740991.0)) };
					}
					RNExecutionScope scope(origin);
					return args[callback_index].getObject(rt).getFunction(rt).call(rt);
				}
				if (name == "getOrigin") {
					const RNExecutionOrigin origin = RNExecutionScope::current();
					if (origin.root_tag == 0) {
						return jsi::Value::null();
					}
					jsi::Object value(rt);
					value.setProperty(rt, "generation", double(origin.generation));
					value.setProperty(rt, "rootTag", origin.root_tag);
					value.setProperty(rt, "epoch", double(origin.surface_epoch));
					return value;
				}
				if (name == "now") {
					return jsi::Value(scheduler->now());
				}
				if (name == "clearTimeout" || name == "clearInterval" || name == "cancelAnimationFrame" || name == "cancelIdleCallback") {
					jsi::Value id = count ? jsi::Value(rt, args[0]) : jsi::Value::undefined();
					if (name == "cancelIdleCallback" && id.isObject()) {
						id = id.getObject(rt).getProperty(rt, "id");
					}
					if (id.isNumber() && std::isfinite(id.getNumber()) && id.getNumber() >= 1 && id.getNumber() <= 9007199254740991.0) {
						scheduler->tasks.erase(uint64_t(id.getNumber()));
					}
					return jsi::Value::undefined();
				}
				const Kind kind = name == "setInterval" ? Kind::INTERVAL : name == "requestAnimationFrame" ? Kind::FRAME
						: name == "requestIdleCallback"													   ? Kind::IDLE
																										   : Kind::TIMEOUT;
				return scheduler->schedule(rt, kind, args, count);
			});
}

void RNRuntimeScheduler::before_runtime_reset_locked(jsi::Runtime &, uint64_t) {
	tasks.clear();
	next_id = 1;
	time_origin = clock();
}
