extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-input"

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

func press(keycode: Key, pressed: bool) -> void:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.physical_keycode = keycode
	key.unicode = keycode if keycode == KEY_SPACE else 0
	key.pressed = pressed
	Input.parse_input_event(key)
	Input.flush_buffered_events()

func services() -> Node:
	for child in get_tree().root.get_children(true):
		if child.name == "RNGameServices":
			return child
	return null

# One event can't say what state an action is in.
func check_action_state() -> void:
	await js("__godotInput.actions.length = 0")
	press(KEY_SPACE, true)
	var motion := InputEventJoypadMotion.new()
	motion.axis = JOY_AXIS_LEFT_X
	motion.axis_value = 0.1
	Input.parse_input_event(motion)
	Input.flush_buffered_events()
	await get_tree().create_timer(0.1).timeout
	var held: Array = await js("__godotInput.actions")
	if held.size() != 1 or not held[0].pressed:
		fail("a small stick move released an action a key still holds: %s" % [held])
		return
	# Godot forgets pressed keys when focus leaves, and sends no release.
	services().notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
	Input.action_release("smoke_jump")
	if not await wait_until(func(): return await js("__godotInput.actions.length === 2 && !__godotInput.actions[1].pressed"), "the release when focus leaves"):
		return
	press(KEY_SPACE, true)
	if not await wait_until(func(): return await js("__godotInput.actions.length === 3 && __godotInput.actions[2].pressed"), "the press after focus returns"):
		return
	press(KEY_SPACE, false)
	await get_tree().create_timer(0.1).timeout

func _run() -> void:
	InputMap.add_action("smoke_jump", 0.5)
	var space := InputEventKey.new()
	space.physical_keycode = KEY_SPACE
	InputMap.action_add_event("smoke_jump", space)
	var stick := InputEventJoypadMotion.new()
	stick.axis = JOY_AXIS_LEFT_X
	stick.axis_value = 1.0
	stick.device = -1
	InputMap.action_add_event("smoke_jump", stick)
	if not await wait_until(func(): return root.get_native_node("field") != null and await js("__godotInput.device === 'keyboard'"), "the app to mount"):
		return
	if not await js("__godotInput.GodotInput.getActions().includes('smoke_jump')"):
		fail("getActions does not list the action")
		return
	var listed: Array = await js("__godotInput.GodotInput.getActionEvents('smoke_jump')")
	if listed.size() != 2 or listed[0].type != "key" or int(listed[0].physicalKeycode) != KEY_SPACE:
		fail("getActionEvents returned %s" % [listed])
		return

	# useInputAction subscribes in an effect, after the first commit.
	await get_tree().process_frame
	await get_tree().process_frame
	press(KEY_SPACE, true)
	press(KEY_SPACE, false)
	if not await wait_until(func(): return await js("__godotInput.actions.length === 2"), "the pressed and released actions"):
		return
	var actions: Array = await js("__godotInput.actions")
	if not actions[0].pressed or actions[1].pressed or actions[0].action != "smoke_jump" or not is_equal_approx(actions[0].strength, 1.0):
		fail("the action events are wrong: %s" % [actions])
		return

	await check_action_state()
	var editor: Control = root.get_native_node("field").find_children("*", "LineEdit", true, false)[0]
	editor.grab_focus()
	await get_tree().process_frame
	await js("__godotInput.actions.length = 0")
	press(KEY_SPACE, true)
	press(KEY_SPACE, false)
	await get_tree().create_timer(0.2).timeout
	if await js("__godotInput.actions.length") != 0:
		fail("a key typed into a TextInput reached the action")
		return
	get_viewport().gui_release_focus()

	await js("__godotInput.captured = null; __godotInput.GodotInput.captureNextEvent({devices: ['keyboard']}).then(event => { __godotInput.captured = event; __godotInput.GodotInput.setActionEvents('smoke_jump', [event]); })")
	await get_tree().create_timer(0.1).timeout
	press(KEY_F, true)
	press(KEY_F, false)
	if not await wait_until(func(): return await js("__godotInput.captured != null"), "the captured event"):
		return
	await get_tree().create_timer(0.1).timeout
	var bound: Array[InputEvent] = InputMap.action_get_events("smoke_jump")
	if bound.size() != 1 or (bound[0] as InputEventKey).physical_keycode != KEY_F:
		fail("rebinding to the captured key failed: %s" % [bound])
		return
	if await js("__godotInput.actions.length") != 0:
		fail("the captured key also fired the action")
		return

	await js("__godotInput.rejected = null; __godotInput.GodotInput.captureNextEvent().catch(error => { __godotInput.rejected = error.code; }); __godotInput.GodotInput.cancelCapture()")
	if not await wait_until(func(): return await js("__godotInput.rejected === 'E_CANCELLED'"), "the cancelled capture"):
		return

	var joypad := InputEventJoypadButton.new()
	joypad.button_index = JOY_BUTTON_A
	joypad.pressed = true
	Input.parse_input_event(joypad)
	Input.flush_buffered_events()
	if not await wait_until(func(): return await js("__godotInput.device === 'joypad'"), "the last device to change"):
		return
	if not await js("(() => { try { __godotInput.GodotInput.getActionEvents('missing'); return false; } catch (error) { return error.code === 'E_VALIDATION'; } })()"):
		fail("an unknown action did not throw E_VALIDATION")
		return
	done()
