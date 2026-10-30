extends Node

# A gamepad drives React views through Godot's ui_* actions: D-pad focus
# follows nextFocus* props, A presses the focused view, and B reaches
# BackHandler only while JavaScript listens for it, and reaches the game when
# no listener handles it.

const WAIT_MSEC := 20000

var failed := false
var unhandled_cancels := 0

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: gamepad-focus: %s" % message)
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

func press(button: JoyButton) -> void:
	for pressed in [true, false]:
		var event := InputEventJoypadButton.new()
		event.button_index = button
		event.pressed = pressed
		get_viewport().push_input(event)

func focused(native_id: String) -> bool:
	var control = root.get_native_node(native_id)
	return control != null and get_viewport().gui_get_focus_owner() == control

func back_presses() -> int:
	return int(await js("__godotGamepad.backPresses"))

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		unhandled_cancels += 1

func bind(action: StringName, button: JoyButton) -> void:
	var event := InputEventJoypadButton.new()
	event.button_index = button
	InputMap.action_add_event(action, event)

# Godot binds no gamepad button to accept or cancel; games add them as Godot
# UIs need them too.
func _ready() -> void:
	bind(&"ui_accept", JOY_BUTTON_A)
	bind(&"ui_cancel", JOY_BUTTON_B)
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return focused("first"), "hasTVPreferredFocus to focus the first button"):
		return
	var first: Control = root.get_native_node("first")
	if not await wait_until(func(): return not first.focus_neighbor_bottom.is_empty(), "nextFocusDown to resolve"):
		return
	press(JOY_BUTTON_DPAD_DOWN)
	if not focused("last"):
		fail("D-pad down did not follow nextFocusDown to the last button")
		return
	press(JOY_BUTTON_A)
	if not await wait_until(func(): return (await js("__godotGamepad.presses")) == ["last"], "A to press the focused button"):
		return

	await js("__godotListenForBack(true)")
	press(JOY_BUTTON_B)
	if not await wait_until(func(): return await back_presses() == 1, "B to reach BackHandler"):
		return
	if unhandled_cancels != 0:
		fail("a handled back press also reached the game")
		return
	await js("__godotGamepad.handlesBack = false")
	press(JOY_BUTTON_B)
	if not await wait_until(func(): return await back_presses() == 2 and unhandled_cancels == 1, "an unhandled back press to reach the game"):
		return
	await js("__godotListenForBack(false)")
	press(JOY_BUTTON_B)
	await get_tree().process_frame
	if unhandled_cancels != 2 or await back_presses() != 2:
		fail("without listeners, B did not stay with the game (%d unhandled, %d back presses)" % [unhandled_cancels, await back_presses()])
		return
	print("RN_SMOKE_OK: gamepad-focus")
	get_tree().quit(0)
