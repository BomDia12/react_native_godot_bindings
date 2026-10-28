extends SceneTree

# Runs a consumer project's main scene and checks that its React root mounted,
# from Metro when RN_CONSUMER_EXPECT_METRO is set and from the bundle file otherwise.

const TEST_ID := "consumer"
const WAIT_MSEC := 60000

func _initialize() -> void:
	var main_scene: PackedScene = load(ProjectSettings.get_setting("application/run/main_scene"))
	root.add_child(main_scene.instantiate())
	_check.call_deferred()

func _check() -> void:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline and not _mounted():
		await process_frame
	var expect_metro := not OS.get_environment("RN_CONSUMER_EXPECT_METRO").is_empty()
	var development: Variant = await ReactNativeHost.evaluate("__DEV__")
	var error := ReactNativeHost.get_last_error()
	if not _mounted():
		print("RN_SMOKE_FAILED: the template root did not mount")
	elif not error.is_empty():
		print("RN_SMOKE_FAILED: %s" % error)
	elif development != expect_metro:
		print("RN_SMOKE_FAILED: expected the %s bundle" % ("Metro" if expect_metro else "file"))
	else:
		print("RN_SMOKE_OK: %s" % TEST_ID)
	quit()

func _mounted() -> bool:
	for view in root.find_children("*", "ReactNativeRootView", true, false):
		var surface := view as ReactNativeRootView
		if surface.get_root_tag() != 0 and surface.get_child(0, true).get_child_count() > 0:
			return true
	return false
