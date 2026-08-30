extends "res://smoke/support/smoke_case.gd"

const SENTINEL := "Baseline commit OK"
const EXPECTED_OFFSET := Vector2(16.0, 16.0)
const TOLERANCE := 0.5

func smoke_id() -> String:
	return "flex-padding"

func validate_smoke() -> String:
	if get_child_count() != 1 or not get_child(0) is Panel:
		return "expected exactly one mounted root Panel"

	var mounted_root := get_child(0) as Panel
	if mounted_root.position.distance_to(Vector2.ZERO) > TOLERANCE:
		return "mounted root Panel is not at the root origin"
	if mounted_root.size.distance_to(size) > TOLERANCE:
		return "mounted root Panel size does not match ReactNativeRootView"
	if mounted_root.get_child_count() != 1 or not mounted_root.get_child(0) is Panel:
		return "expected one styled View Panel below the mounted root"

	var flex_panel := mounted_root.get_child(0) as Panel
	if flex_panel.position.distance_to(Vector2.ZERO) > TOLERANCE:
		return "flex View is not at the mounted root origin"
	if flex_panel.size.distance_to(mounted_root.size) > TOLERANCE:
		return "flex View size does not fill the mounted root"
	if flex_panel.size.x <= TOLERANCE or flex_panel.size.y <= TOLERANCE:
		return "flex View has an empty layout"
	if flex_panel.get_child_count() != 1 or not flex_panel.get_child(0) is Label:
		var child_types: Array[String] = []
		for child in flex_panel.get_children():
			child_types.append(child.get_class())
		return "expected the sentinel Label as the direct flex View child, got %s" % ", ".join(child_types)

	var label := flex_panel.get_child(0) as Label
	if label.text != SENTINEL:
		return "direct Label does not contain the sentinel text"
	if label.position.distance_to(EXPECTED_OFFSET) > TOLERANCE:
		return "padding did not place the sentinel Label at (16, 16)"
	if label.size.x <= TOLERANCE or label.size.y <= TOLERANCE:
		return "sentinel Label has an empty layout"

	return ""
