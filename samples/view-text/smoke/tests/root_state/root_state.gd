extends Node

# Pausing the tree suspends JavaScript timers while no root can process, and
# each root reports its visibility and processing to useGodotRootState().

const WAIT_MSEC := 20000
const TIMER_MSEC := 300

var failed := false

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: root-state: %s" % message)
		get_tree().paused = false
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func state(label: String) -> Dictionary:
	var states = await js("__godotRootStates")
	return states.get(label, {}) if states is Dictionary else {}

func fired() -> int:
	return int(await js("__godotTimers.fired"))

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func wait_msec(duration: int) -> void:
	var end := Time.get_ticks_msec() + duration
	while Time.get_ticks_msec() < end:
		await get_tree().process_frame

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	var pausable: ReactNativeRootView = $Pausable
	if not await wait_until(func(): return (await state("pausable")).get("processing") == true, "the initial root state"):
		return
	pausable.hide()
	if not await wait_until(func(): return (await state("pausable")).get("visible") == false, "the hidden state"):
		return
	pausable.show()

	await js("__godotStartTimer(%d)" % TIMER_MSEC)
	get_tree().paused = true
	if not await wait_until(func(): return (await state("pausable")).get("processing") == false, "the paused state"):
		return
	await wait_msec(TIMER_MSEC * 3)
	if await fired() != 0:
		fail("a timer fired while every root was paused")
		return
	get_tree().paused = false
	if not await wait_until(func(): return await fired() == 1, "the timer to resume"):
		return

	var always := ReactNativeRootView.new()
	always.process_mode = Node.PROCESS_MODE_ALWAYS
	always.application_key = "GodotRootStateApp"
	always.initial_properties = {"label": "always"}
	always.size = Vector2(200, 100)
	add_child(always)
	if not await wait_until(func(): return (await state("always")).get("processing") == true, "the always-processing root"):
		return
	await js("__godotStartTimer(%d)" % TIMER_MSEC)
	get_tree().paused = true
	if not await wait_until(func(): return await fired() == 1, "a timer while a root still processes"):
		return
	if (await state("pausable")).get("processing") != false:
		fail("the pausable root still reports processing while paused")
		return
	get_tree().paused = false
	print("RN_SMOKE_OK: root-state")
	get_tree().quit(0)
