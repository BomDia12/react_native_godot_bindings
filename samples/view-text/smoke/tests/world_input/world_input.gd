extends Node

const WAIT_MSEC := 20000
const TEST_ID := "world-input"

var failed := false

@onready var root: ReactNativeRootView = $Root
@onready var world: Node2D = $World

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

func settle() -> void:
	await get_tree().create_timer(0.2).timeout

func rect_of(native_id: String) -> Rect2:
	return (root.get_native_node(native_id) as Control).get_global_rect()

func mouse(at: Vector2, pressed: bool, button := MOUSE_BUTTON_LEFT) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = pressed
	event.position = at
	event.global_position = at
	if pressed and button == MOUSE_BUTTON_LEFT:
		event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func motion(at: Vector2, held := false) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	if held:
		event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func touch(at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = 0
	event.position = at
	event.pressed = pressed
	get_viewport().push_input(event)

func click(at: Vector2) -> void:
	mouse(at, true)
	mouse(at, false)

func tap(at: Vector2) -> void:
	touch(at, true)
	touch(at, false)

func reached_game(label: String, presses: int) -> bool:
	await settle()
	if world.presses != presses or world.releases != presses:
		return fail("%s: the game counted %d presses and %d releases, not %d" % [label, world.presses, world.releases, presses])
	world.reset()
	return true

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return root.get_native_node("box_only_child") != null, "the views to mount"):
		return
	await settle()
	world.reset()

	var empty := Vector2(700, 580)
	click(empty)
	tap(empty)
	if not await reached_game("empty space", 2):
		return
	var box_none := rect_of("box_none").end - Vector2(20, 20)
	click(box_none)
	tap(box_none)
	if not await reached_game("a box-none view", 2):
		return

	var pressable := rect_of("pressable").get_center()
	click(pressable)
	if not await wait_until(func(): return (await js("__godotWorld.events")).count("press") == 1, "the Pressable's onPress"):
		return
	tap(pressable)
	if not await wait_until(func(): return (await js("__godotWorld.events")).count("press") == 2, "the Pressable's touch onPress"):
		return
	if not await reached_game("a Pressable", 0):
		return
	click(rect_of("opaque").get_center())
	tap(rect_of("opaque").get_center())
	if not await reached_game("an opaque view", 0):
		return

	# A box-only view's children are not targets.
	click(rect_of("box_only_child").get_center())
	if not await wait_until(func(): return (await js("__godotWorld.events")).has("outer"), "the box-only Pressable's onPress"):
		return
	if (await js("__godotWorld.events")).has("inner"):
		fail("a child of a box-only view received a press")
		return

	var row := rect_of("row_1").get_center()
	for i in 3:
		mouse(row, true, MOUSE_BUTTON_WHEEL_DOWN)
		mouse(row, false, MOUSE_BUTTON_WHEEL_DOWN)
	if not await wait_until(func(): return float(await js("__godotWorld.scrollY")) > 0, "the ScrollView to scroll"):
		return
	await settle()
	if world.wheels != 0:
		fail("the game counted %d wheel events over a ScrollView" % world.wheels)
		return

	var slider := rect_of("slider")
	var start := slider.position + Vector2(5, slider.size.y / 2)
	mouse(start, true)
	motion(start + Vector2(100, 0), true)
	mouse(start + Vector2(100, 0), false)
	if not await wait_until(func(): return float(await js("__godotWorld.slider ?? 0")) > 0.2, "the Slider to follow the drag"):
		return
	if not await reached_game("a Slider drag", 0):
		return

	click(rect_of("input").get_center())
	await settle()
	var focused := get_viewport().gui_get_focus_owner()
	if not focused is LineEdit or not root.get_native_node("input").is_ancestor_of(focused):
		fail("a click did not focus the TextInput's editor: %s" % focused)
		return
	if not await reached_game("a TextInput click", 0):
		return

	world.reset()
	motion(pressable)
	await settle()
	if world.motions == 0:
		fail("mouse motion over a Pressable did not reach the game")
		return
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)
