extends "res://smoke/support/smoke_case.gd"

const SENTINEL := "Baseline commit OK"
const EXPECTED_BACKGROUND := Color("123456")
const EXPECTED_TEXT_COLOR := Color("00ff00")
const EXPECTED_FONT_SIZE := 20
const EXPECTED_OPACITY := 0.8

func smoke_id() -> String:
	return "baseline-mount"

func find_sentinel(node: Node) -> Label:
	if node is Label and node.text == SENTINEL:
		return node

	for child in node.get_children():
		var label := find_sentinel(child)
		if label != null:
			return label

	return null

func find_styled_panel(node: Node) -> Panel:
	if node is Panel:
		var style := node.get_theme_stylebox("panel") as StyleBoxFlat
		if style != null and style.bg_color.is_equal_approx(EXPECTED_BACKGROUND):
			return node

	for child in node.get_children():
		var panel := find_styled_panel(child)
		if panel != null:
			return panel

	return null

func has_only_expected_node_types(node: Node) -> bool:
	for child in node.get_children():
		if not child is Panel and not child is Label:
			return false
		if not has_only_expected_node_types(child):
			return false

	return true

func validate_smoke() -> String:
	if get_child_count() != 1 or not get_child(0) is Panel:
		return "expected exactly one mounted root Panel"

	var mounted_root := get_child(0) as Panel
	if not has_only_expected_node_types(self):
		return "mounted tree contains an unexpected native node type"

	var label := find_sentinel(mounted_root)
	if label == null:
		return "sentinel Label was not mounted"

	var styled_panel := find_styled_panel(mounted_root)
	if styled_panel == null:
		return "styled View panel was not mounted"
	if styled_panel.size.x <= 0.0 or styled_panel.size.y <= 0.0:
		return "styled View has an empty Yoga layout"
	if label.size.x <= 0.0 or label.size.y <= 0.0:
		return "sentinel Text has an empty Yoga layout"
	if not label.get_theme_color("font_color").is_equal_approx(EXPECTED_TEXT_COLOR):
		return "sentinel Text color does not match the fixture"
	if label.get_theme_font_size("font_size") != EXPECTED_FONT_SIZE:
		return "sentinel Text font size does not match the fixture"
	if not is_equal_approx(styled_panel.modulate.a, EXPECTED_OPACITY):
		return "styled View opacity does not match the fixture"

	return ""
