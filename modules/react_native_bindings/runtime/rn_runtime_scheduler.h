#pragma once

#include "../singletons/hermes_runtime_lifecycle.h"
#include "rn_execution_scope.h"

#include <jsi/jsi.h>

#include <functional>
#include <map>
#include <memory>
#include <vector>

class RNRuntimeScheduler : public facebook::jsi::HostObject, public HermesRuntimeLifecycle, public std::enable_shared_from_this<RNRuntimeScheduler> {
public:
	enum class Kind { TIMEOUT,
		INTERVAL,
		FRAME,
		IDLE };
	using Clock = std::function<double()>;

private:
	struct Task {
		RNExecutionOrigin origin;
		Kind kind = Kind::TIMEOUT;
		double due = 0;
		double interval = 0;
		std::unique_ptr<facebook::jsi::Function> callback;
		std::vector<facebook::jsi::Value> arguments;
	};
	Clock clock;
	double time_origin = 0;
	uint64_t next_id = 1;
	std::map<uint64_t, Task> tasks;
	size_t max_tasks = 256;
	size_t frame_remaining = 256;
	double idle_budget = 2;
	bool draining = false;

	facebook::jsi::Value schedule(facebook::jsi::Runtime &p_runtime, Kind p_kind, const facebook::jsi::Value *p_arguments, size_t p_count);
	void drain(facebook::jsi::Runtime &p_runtime, bool p_idle, bool p_visual_frame, const std::function<void()> &p_checkpoint, double p_available_ms = 2);

public:
	explicit RNRuntimeScheduler(Clock p_clock = {});
	double now() const;
	void configure(size_t p_max_tasks, double p_idle_budget);
	bool has_pending_work() const { return !tasks.empty(); }
	void process_frame_locked(facebook::jsi::Runtime &p_runtime, bool p_visual_frame, const std::function<void()> &p_checkpoint, size_t p_native_delivered = 0);
	void process_idle_locked(facebook::jsi::Runtime &p_runtime, const std::function<void()> &p_checkpoint, double p_available_ms = 2);
	facebook::jsi::Value get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) override;
	void before_runtime_reset_locked(facebook::jsi::Runtime &p_runtime, uint64_t p_generation) override;
};
