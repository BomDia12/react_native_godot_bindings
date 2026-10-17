extends "res://smoke/support/smoke_case.gd"

const SENTINEL := "Baseline commit OK"
const EXPECTED_OFFSET := Vector2(16.0, 16.0)
const TOLERANCE := 0.5

func smoke_id() -> String:
	return "flex-padding"

func validate_smoke() -> String:
	var container := mount_container()
	if container.get_child_count() != 1 or not container.get_child(0) is Panel:
		return "expected the flex View Panel as the only mounted child; React Native flattens the layout-only wrappers"

	var flex_panel := container.get_child(0) as Panel
	if flex_panel.position.distance_to(Vector2.ZERO) > TOLERANCE:
		return "flex View is not at the root origin"
	if flex_panel.size.distance_to(size) > TOLERANCE:
		return "flex View size does not fill ReactNativeRootView"
	if flex_panel.get_child_count() != 1 or not flex_panel.get_child(0) is RNTextControl:
		var child_types: Array[String] = []
		for child in flex_panel.get_children():
			child_types.append(child.get_class())
		return "expected the sentinel RNTextControl as the direct flex View child, got %s" % ", ".join(child_types)

	var label := flex_panel.get_child(0) as RNTextControl
	if label.get_text() != SENTINEL:
		return "direct RNTextControl does not contain the sentinel text"
	if label.position.distance_to(EXPECTED_OFFSET) > TOLERANCE:
		return "padding did not place the sentinel RNTextControl at (16, 16)"
	if label.size.x <= TOLERANCE or label.size.y <= TOLERANCE:
		return "sentinel RNTextControl has an empty layout"

	return ""
