extends SceneTree

# Runs a consumer project's main scene and checks that its React root mounted,
# from Metro when RN_CONSUMER_EXPECT_METRO is set and from the bundle file
# otherwise, that JavaScript sees where its bundle came from, and that the
# autolinked script library's module answers JavaScript.

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
	var script_url: Variant = await ReactNativeHost.evaluate("nativeModuleProxy.SourceCode.getConstants().scriptURL")
	var expected_source: bool = String(script_url).begins_with("http://") if expect_metro else script_url == ProjectSettings.get_setting("react_native/bundle/path")
	var score: Variant = await ReactNativeHost.evaluate("__consumerScore()")
	var error := ReactNativeHost.get_last_error()
	if not _mounted():
		print("RN_SMOKE_FAILED: the template root did not mount")
	elif not error.is_empty():
		print("RN_SMOKE_FAILED: %s" % error)
	elif development != expect_metro:
		print("RN_SMOKE_FAILED: expected the %s bundle" % ("Metro" if expect_metro else "file"))
	elif not expected_source:
		print("RN_SMOKE_FAILED: SourceCode.scriptURL is %s" % script_url)
	elif score != 1:
		print("RN_SMOKE_FAILED: the linked Scoreboard module answered %s" % score)
	else:
		print("RN_SMOKE_OK: %s" % TEST_ID)
	quit()

func _mounted() -> bool:
	for view in root.find_children("*", "ReactNativeRootView", true, false):
		var surface := view as ReactNativeRootView
		if surface.get_root_tag() != 0 and surface.get_child(0, true).get_child_count() > 0:
			return true
	return false
