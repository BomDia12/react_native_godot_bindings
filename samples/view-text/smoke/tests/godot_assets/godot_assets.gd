extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "godot-assets"

func validate_smoke() -> String:
	var state = HermesRuntime.get_global("__godotAssetState")
	if not state is Dictionary:
		return "asset fixture state is missing"
	var primary = state.get("primary", {})
	var secondary = state.get("secondary", {})
	if not String(primary.get("uri", "")).begins_with("res://dist/assets/"):
		return "primary asset did not resolve to staged res:// URI"
	if primary.get("scale", 0.0) < 1.0 or primary.get("width") != 24.0 or primary.get("height") != 24.0:
		return "primary asset metadata is incorrect: %s" % [primary]
	if primary.get("uri") == secondary.get("uri"):
		return "same-basename assets collided"
	if state.get("explicitUser", {}).get("uri") != "user://rn-godot-asset-smoke.txt":
		return "explicit user:// source was not preserved"
	if state.get("invalidLocalSourcesRejected") != 5:
		return "invalid local asset sources were not all rejected"
	var missing_uri: String = state.get("missing", {}).get("uri", "")
	if missing_uri != "res://missing-phase-5-asset.png" or FileAccess.file_exists(missing_uri) or ResourceLoader.exists(missing_uri):
		return "missing explicit local asset did not remain unavailable"
	var primary_uri: String = primary.get("uri", "")
	if not FileAccess.file_exists(primary_uri):
		return "staged asset bytes are missing"
	var texture = ResourceLoader.load(primary_uri)
	if not texture is Texture2D or texture.get_width() != int(24 * primary.scale):
		return "ResourceLoader did not import the selected PNG dimensions"
	var user_uri := "user://rn-godot-asset-smoke.txt"
	var file := FileAccess.open(user_uri, FileAccess.WRITE)
	if file == null:
		return "could not create test-owned user:// file"
	file.store_string("asset-smoke")
	file.close()
	if FileAccess.get_file_as_string(user_uri) != "asset-smoke":
		return "could not read test-owned user:// file"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(user_uri))
	return ""
