extends Node

const WAIT_MSEC := 20000

var failed := false
var loaded := {}

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: godot-scene: %s" % message)
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

func nested_mounted(panel: Node) -> bool:
	var nested: ReactNativeRootView = panel.get_node("Nested")
	return nested.get_root_tag() != 0 and nested.get_child(0, true).get_child_count() > 0

func _ready() -> void:
	root.scene_loaded.connect(func(native_id: String, instance: Node): loaded[native_id] = instance)
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return loaded.size() == 2 and (await js("__godotScenes.loads.length")) == 2, "both scenes to load"):
		return
	var panel: Node = loaded["panel"]
	var sprite: Node = loaded["sprite"]
	if root.get_native_node("panel") != panel or root.get_native_node("sprite") != sprite:
		fail("get_native_node did not return the loaded instances")
		return
	if not panel is Control or not is_equal_approx((panel as Control).size.x, 100.0):
		fail("the Control scene does not fill its GodotScene")
		return
	if not sprite is Node2D or not sprite.get_parent() is SubViewport:
		fail("the Node2D scene is not shown through a SubViewport")
		return
	if not await wait_until(func(): return nested_mounted(panel), "the root nested in the scene to mount"):
		return

	var field: LineEdit = panel.get_node("Field")
	field.grab_focus()
	var key := InputEventKey.new()
	key.keycode = KEY_A
	key.unicode = "a".unicode_at(0)
	key.pressed = true
	get_viewport().push_input(key)
	await get_tree().process_frame
	if field.text != "a":
		fail("the scene's LineEdit did not receive its key")
		return

	await js("__godotSetSceneSource('res://smoke/tests/godot_scene/Replacement.tscn')")
	if not await wait_until(func(): return is_instance_valid(loaded["panel"]) and loaded["panel"] is ColorRect, "the replacement scene"):
		return
	await get_tree().process_frame
	if is_instance_valid(panel):
		fail("the previous scene instance was not freed")
		return
	print("RN_SMOKE_OK: godot-scene")
	get_tree().quit(0)
