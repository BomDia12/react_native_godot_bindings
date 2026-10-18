extends SceneTree

const TEST_ID := "game-staged"
const ROOT_COUNT := 4
const WAIT_MSEC := 30000

func _initialize() -> void:
	var main_scene: PackedScene = load(ProjectSettings.get_setting("application/run/main_scene"))
	root.add_child(main_scene.instantiate())
	_check.call_deferred()

func _check() -> void:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline and _mounted_roots() < ROOT_COUNT:
		await process_frame
	var mounted := _mounted_roots()
	var error := ReactNativeHost.get_last_error()
	if mounted != ROOT_COUNT:
		print("RN_SMOKE_FAILED: %d of %d roots mounted" % [mounted, ROOT_COUNT])
	elif not error.is_empty():
		print("RN_SMOKE_FAILED: %s" % error)
	else:
		print("RN_SMOKE_OK: %s" % TEST_ID)
	quit()

func _mounted_roots() -> int:
	var mounted := 0
	for view in root.find_children("*", "ReactNativeRootView", true, false):
		var surface := view as ReactNativeRootView
		if surface.get_root_tag() != 0 and surface.get_child_count(true) > 0 and surface.get_child(0, true).get_child_count() > 0:
			mounted += 1
	return mounted
