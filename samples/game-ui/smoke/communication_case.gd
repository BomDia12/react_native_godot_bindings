extends Node

# Shared steps for the scenes that check how roots talk to GDScript.

const WAIT_MSEC := 20000

var failed := false

func smoke_id() -> String:
	return ""

func run() -> void:
	pass

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [smoke_id(), message])
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

func add_root(key: String) -> ReactNativeRootView:
	var root := ReactNativeRootView.new()
	root.application_key = key
	root.size = Vector2(300, 200)
	add_child(root)
	return root

func mounted(root: ReactNativeRootView) -> bool:
	return root.get_root_tag() != 0 and root.get_child(0, true).get_child_count() > 0

func _ready() -> void:
	ReactNativeFileSingleton.set_monitored_file("res://dist/game.bundle.js")
	_run.call_deferred()

func _run() -> void:
	await run()
	if not failed:
		print("RN_SMOKE_OK: %s" % smoke_id())
		get_tree().quit(0)
