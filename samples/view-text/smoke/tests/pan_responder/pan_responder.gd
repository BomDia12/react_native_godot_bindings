extends Node

const WAIT_MSEC := 20000
const TEST_ID := "pan-responder"

var failed := false

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func done() -> void:
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func _ready() -> void:
	_run.call_deferred()

func mouse(at: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	event.position = at
	event.global_position = at
	if pressed:
		event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func move(at: Vector2, relative: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	event.relative = relative
	event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func touch(at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = 0
	event.position = at
	event.pressed = pressed
	get_viewport().push_input(event)

func touch_move(at: Vector2, relative: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = 0
	event.position = at
	event.relative = relative
	get_viewport().push_input(event)

func check_gesture(label: String) -> bool:
	var names: Array = await js("__godotPan.events.map(event => event[0])")
	if names.size() < 3 or names[0] != "grant" or names[-1] != "release" or not names.has("move"):
		fail("%s: the responder events are wrong: %s" % [label, names])
		return false
	var last: Dictionary = await js("__godotPan.events[__godotPan.events.length - 1][1]")
	if not is_equal_approx(last.dx, 60.0) or not is_equal_approx(last.dy, 30.0):
		fail("%s: the release's gesture is %s" % [label, last])
		return false
	var moved: Dictionary = await js("__godotPan.events[__godotPan.events.length - 2][1]")
	if int(moved.numberActiveTouches) != 1 or not is_finite(moved.vx) or moved.vx <= 0.0:
		fail("%s: the move's gesture is %s" % [label, moved])
		return false
	return true

func _run() -> void:
	if not await wait_until(func(): return root.get_native_node("pan") != null, "the view to mount"):
		return
	await get_tree().create_timer(0.2).timeout
	var start := (root.get_native_node("pan") as Control).get_global_rect().position + Vector2(20, 20)

	mouse(start, true)
	await get_tree().create_timer(0.05).timeout
	for i in range(1, 4):
		move(start + Vector2(20, 10) * i, Vector2(20, 10))
		await get_tree().create_timer(0.05).timeout
	mouse(start + Vector2(60, 30), false)
	if not await wait_until(func(): return await js("__godotPan.events.some(event => event[0] === 'release')"), "the mouse release"):
		return
	if not await check_gesture("mouse"):
		return

	await js("__godotPan.events.length = 0")
	touch(start, true)
	await get_tree().create_timer(0.05).timeout
	for i in range(1, 4):
		touch_move(start + Vector2(20, 10) * i, Vector2(20, 10))
		await get_tree().create_timer(0.05).timeout
	touch(start + Vector2(60, 30), false)
	if not await wait_until(func(): return await js("__godotPan.events.some(event => event[0] === 'release')"), "the touch release"):
		return
	if not await check_gesture("touch"):
		return

	# A drag taking over the press ends the gesture.
	await js("__godotPan.events.length = 0")
	var draggable := (root.get_native_node("pan_drag") as Control).get_global_rect().position + Vector2(20, 20)
	mouse(draggable, true)
	await get_tree().create_timer(0.05).timeout
	for i in range(1, 4):
		move(draggable + Vector2(20, 0) * i, Vector2(20, 0))
		await get_tree().create_timer(0.05).timeout
	if not await wait_until(func(): return await js("__godotPan.events.some(event => event[0] === 'terminate')"), "the gesture to terminate"):
		return
	mouse(draggable + Vector2(60, 0), false)
	done()
