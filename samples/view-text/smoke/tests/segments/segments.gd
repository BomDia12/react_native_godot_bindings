extends Node

# A startup segment from react_native/bundle/segments registers a component
# before roots start, ReactNativeHost.load_segment adds another at runtime,
# and both are evaluated again, in order, after a hard reload.

const WAIT_MSEC := 20000
const BUNDLE := "res://dist/segments/index.bundle.js"

var failed := false

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: segments: %s" % message)
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
	root.size = Vector2(300, 100)
	add_child(root)
	return root

func mounted(root: ReactNativeRootView) -> bool:
	return root.get_root_tag() != 0 and root.get_child(0, true).get_child_count() > 0

# The runtime reads its segments when it starts, which the first root does.
func _ready() -> void:
	ReactNativeFileSingleton.set_monitored_file(BUNDLE)
	ProjectSettings.set_setting("react_native/bundle/segments", PackedStringArray(["startup_segment"]))
	_run.call_deferred()

func _run() -> void:
	var startup := add_root("StartupSegmentApp")
	if not await wait_until(func(): return mounted(startup), "the startup segment's component"):
		return
	if not await ReactNativeHost.load_segment(BUNDLE.get_base_dir().path_join("runtime_segment.segment.js")):
		fail("load_segment did not report success")
		return
	var runtime := add_root("RuntimeSegmentApp")
	if not await wait_until(func(): return mounted(runtime), "the runtime segment's component"):
		return
	if await js("__segments") != ["startup", "runtime"]:
		fail("segments ran as %s" % [await js("__segments")])
		return
	if await ReactNativeHost.load_segment("res://dist/segments/missing.segment.js"):
		fail("a missing segment reported success")
		return

	var generation := ReactNativeHost.get_generation()
	ReactNativeHost.reload()
	if not await wait_until(func(): return ReactNativeHost.get_generation() != generation and mounted(startup) and mounted(runtime), "both segment roots after a reload"):
		return
	if not await wait_until(func(): return await js("__segments") == ["startup", "runtime"], "both segments to run again, in order"):
		return
	print("RN_SMOKE_OK: segments")
	get_tree().quit(0)
