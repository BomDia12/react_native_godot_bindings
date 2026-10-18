extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "godot-platform"

func validate_smoke() -> String:
	var state = await js("__godotPlatformState")
	if not state is Dictionary:
		return "platform fixture state is missing"
	if state.get("os") != "godot" or state.get("selected") != "godot":
		return "Godot platform identity or selection failed"
	if not state.get("explicitUndefinedPreserved", false):
		return "Platform.select did not preserve explicit undefined"
	if state.get("unsupportedCode") != "E_UNSUPPORTED":
		return "BackHandler.exitApp did not fail explicitly"
	if not state.get("backListenerRemovable", false):
		return "BackHandler listeners did not return a removable subscription"
	var constants = state.get("constants", {})
	if constants.get("reactNativeVersion", {}).get("minor") != 87:
		return "native PlatformConstants did not report the pinned RN version"
	if constants.get("assetScale", 0.0) <= 0.0:
		return "native asset scale is invalid"
	var microtasks = state.get("microtasks", [])
	if microtasks != ["sync", "promise", "native"]:
		return "microtask checkpoint order was %s" % [microtasks]
	if mount_container().get_child_count() < 1:
		return "platform React tree did not mount"
	return ""
