extends "res://smoke/support/smoke_case.gd"

const WAIT_MSEC := 10000

func smoke_id() -> String:
	return "animation-frames"

func result_of(name: String) -> Dictionary:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		var result = await js("__godotFrames.%s" % name)
		if result is Dictionary and result.get("done", false):
			return result
		await next_frame()
	return {}

func validate_smoke():
	await js("__godotFrames.count(2000)")
	var counted := await result_of("counted")
	# Upstream's setTimeout(0) runs thousands of callbacks in 2 s; the lower
	# bound leaves room for stalls on contended runners.
	var callbacks := int(counted.get("callbacks", 0))
	if callbacks < 90 or callbacks > 126:
		return "requestAnimationFrame ran %d callbacks in 2 s, not about 120" % callbacks

	await js("__godotFrames.order()")
	var ordered := await result_of("ordered")
	if ordered.is_empty():
		return "the ordering callbacks did not run"
	var timestamps: Array = ordered.timestamps
	if timestamps.size() != 3 or timestamps[0] != timestamps[1] or timestamps[1] != timestamps[2]:
		return "callbacks of one frame did not share a timestamp: %s" % [timestamps]
	if not float(ordered.nested) > float(timestamps[0]):
		return "a callback registered during a frame ran in the same frame: %s" % [ordered]
	if ordered.cancelledRan:
		return "cancelAnimationFrame did not prevent a callback"

	await js("__godotFrames.busy()")
	var stalled := await result_of("stalled")
	var runs: Array = stalled.get("runs", [])
	if runs.size() != 4 or float(runs[1]) - float(runs[0]) < 190:
		return "the busy callback did not hold the JS thread: %s" % [runs]
	# One frame runs once the thread is free, and the next ones wait for the
	# clock, so the two after it are at least one period apart.
	if float(runs[3]) - float(runs[1]) < 12:
		return "a busy JS thread was followed by a burst of frames: %s" % [runs]

	await js("__godotFrames.decay()")
	var decayed := await result_of("decayed")
	# Upstream's decay travels velocity / (1 - deceleration) points.
	var distance := float(decayed.get("value", 0))
	if absf(distance - 500.0) > 25.0:
		return "a JS-driven decay travelled %f points, not 500" % distance

	await js("__godotFrames.throwing()")
	if (await result_of("thrown")).is_empty():
		return "a throwing callback stopped the rest of its frame"
	if not ReactNativeHost.get_last_error().contains("intentional frame callback failure"):
		return "a frame callback's error did not reach the host: %s" % ReactNativeHost.get_last_error()
	return ""
