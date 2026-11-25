extends Node

const WAIT_MSEC := 20000
const TEST_ID := "multi-touch"

var failed := false

@onready var left: ReactNativeRootView = $Left
@onready var right: ReactNativeRootView = $Right

func fail(message: String) -> bool:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)
	return false

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	return fail("timed out waiting for %s" % label)

func events(side: String) -> Array:
	return await js("__godotMultiTouch.%s" % side)

func first(side: String, type: String) -> Dictionary:
	for event in await events(side):
		if event.type == type:
			return event
	return {}

func has(side: String, type: String) -> bool:
	return not (await first(side, type)).is_empty()

func center(root: ReactNativeRootView) -> Vector2:
	return (root.get_native_node("touch_target") as Control).get_global_rect().get_center()

func touch(index: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	get_viewport().push_input(event)

func drag(index: int, at: Vector2, relative: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = at
	event.relative = relative
	get_viewport().push_input(event)

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return left.get_native_node("touch_target") != null and right.get_native_node("touch_target") != null, "both roots to mount"):
		return
	await get_tree().create_timer(0.2).timeout
	var left_point := center(left)
	touch(0, left_point, true)
	if not await wait_until(func(): return await has("left", "touchStart"), "the left root's touch"):
		return
	touch(1, center(right), true)
	if not await wait_until(func(): return await has("right", "grant"), "the right root's grant"):
		return

	# The right root's events list the contact the left root holds.
	var started := await first("right", "touchStart")
	if started.get("touches", []) != [0, 1]:
		fail("a touch in one root omitted the other root's contact: %s" % [started])
		return
	var granted := await first("right", "grant")
	if int(granted.get("active", 0)) != 2:
		fail("PanResponder counted %s active touches across two roots" % granted.get("active"))
		return
	if not (await first("left", "pointerDown")).get("primary", false) or (await first("right", "pointerDown")).get("primary", true):
		fail("only the application's first contact should be primary: %s %s" % [await first("left", "pointerDown"), await first("right", "pointerDown")])
		return

	# Unmounting the right root cancels its contact only.
	remove_child(right)
	right.queue_free()
	drag(0, left_point + Vector2(10, 0), Vector2(10, 0))
	if not await wait_until(func(): return await has("left", "touchMove"), "the left root's touch move"):
		return
	if await has("left", "touchCancel") or await has("left", "pointerCancel"):
		fail("unmounting one root cancelled another root's contact: %s" % [await events("left")])
		return
	var moved := await first("left", "touchMove")
	if moved.get("touches", []) != [0]:
		fail("the unmounted root's contact stayed in the touch list: %s" % [moved])
		return
	touch(0, left_point + Vector2(10, 0), false)
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)
