extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-drag"

var failed := false

# A window only tracks the control under the mouse once the OS reports the mouse
# inside it, which never happens headless. A SubViewport tracks it from events.
@onready var host: SubViewport = $Host
@onready var root: ReactNativeRootView = $Host/Root

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

var gdscript_dropped: Variant = null

func mouse(at: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	event.position = at
	event.global_position = at
	if pressed:
		event.button_mask = MOUSE_BUTTON_MASK_LEFT
	host.push_input(event)

func move(at: Vector2, relative: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	event.relative = relative
	event.button_mask = MOUSE_BUTTON_MASK_LEFT
	host.push_input(event)

func drag(from: Vector2, to: Vector2) -> void:
	mouse(from, true)
	await get_tree().process_frame
	var step := (to - from) / 4.0
	for i in range(1, 5):
		move(from + step * i, step)
		await get_tree().process_frame
	mouse(to, false)
	await get_tree().process_frame

func center(native_id: String) -> Vector2:
	return (root.get_native_node(native_id) as Control).get_global_rect().get_center()

func _run() -> void:
	if not await wait_until(func(): return root.get_native_node("refuses") != null and await js("__godotDrag.drops != null"), "the views to mount"):
		return
	host.notify_mouse_entered()
	# The preview texture loads on a loader thread.
	await get_tree().create_timer(0.2).timeout

	if not root.get_native_node("source").is_ancestor_of(root.get_native_node("source_child")) or not root.get_native_node("target").is_ancestor_of(root.get_native_node("target_child")):
		fail("a drag source or target has its children hoisted out of it")
		return
	var started := []
	root.godot_drag_started.connect(func(data): started.append(data))
	await drag(center("source"), center("target"))
	if not await wait_until(func(): return await js("__godotDrag.drops.length === 1 && __godotDrag.ends.length === 1"), "the drop and the drag's end"):
		return
	var drop: Dictionary = await js("__godotDrag.drops[0]")
	if drop.data.type != "item" or int(drop.data.id) != 7:
		fail("the drop carried %s" % drop)
		return
	if not (await js("__godotDrag.ends[0].successful")):
		fail("the source did not hear of the success")
		return
	if started.size() != 1 or started[0].type != "item":
		fail("the root did not report the drag: %s" % [started])
		return
	if not await wait_until(func(): return await js("__godotDrag.seen.length > 0 && __godotDrag.seen[0].type === 'item'"), "useGodotDrag to see the drag"):
		return

	await drag(center("source"), center("refuses"))
	if not await wait_until(func(): return await js("__godotDrag.ends.length === 2"), "the refused drag's end"):
		return
	if (await js("__godotDrag.ends[1].successful")) or (await js("__godotDrag.drops.length")) != 1:
		fail("a view accepted a drag type it does not list")
		return

	# Into a GDScript control, which reads the same Dictionary.
	var catcher: Control = preload("res://smoke/tests/game_drag/catcher.gd").new()
	catcher.position = Vector2(300, 300)
	catcher.size = Vector2(80, 80)
	catcher.dropped.connect(func(data): gdscript_dropped = data)
	host.add_child(catcher)
	await get_tree().process_frame
	await drag(center("source"), catcher.get_global_rect().get_center())
	if not await wait_until(func(): return gdscript_dropped != null, "the GDScript control's drop"):
		return
	if gdscript_dropped.type != "item":
		fail("the GDScript control received %s" % [gdscript_dropped])
		return

	# And from one into a React view.
	catcher.outgoing = {"type": "item", "id": 9}
	await drag(catcher.get_global_rect().get_center(), center("target"))
	if not await wait_until(func(): return await js("__godotDrag.drops.length === 2"), "the drop from GDScript"):
		return
	if int(await js("__godotDrag.drops[1].data.id")) != 9:
		fail("the React view received %s" % (await js("JSON.stringify(__godotDrag.drops[1])")))
		return
	done()
