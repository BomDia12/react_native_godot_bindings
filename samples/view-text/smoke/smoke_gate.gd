extends ReactNativeRootView

const SENTINEL := "Baseline commit OK"
const TIMEOUT_FRAMES := 600
const SETTLE_FRAMES := 30
const EXPECTED_BACKGROUND := Color("123456")
const EXPECTED_TEXT_COLOR := Color("00ff00")
const EXPECTED_FONT_SIZE := 20
const EXPECTED_OPACITY := 0.8

var frames := 0
var settle_frames := -1

func _fail(message: String) -> void:
	printerr("RN_BASELINE_FAILED: %s" % message)
	get_tree().quit(1)

func _find_sentinel(node: Node) -> Label:
	if node is Label and node.text == SENTINEL:
		return node

	for child in node.get_children():
		var label := _find_sentinel(child)
		if label != null:
			return label

	return null

func _find_styled_panel(node: Node) -> Panel:
	if node is Panel:
		var style := node.get_theme_stylebox("panel") as StyleBoxFlat
		if style != null and style.bg_color.is_equal_approx(EXPECTED_BACKGROUND):
			return node

	for child in node.get_children():
		var panel := _find_styled_panel(child)
		if panel != null:
			return panel

	return null

func _validate_node_types(node: Node) -> bool:
	for child in node.get_children():
		if not child is Panel and not child is Label:
			return false
		if not _validate_node_types(child):
			return false

	return true

func _validate() -> void:
	if get_child_count() != 1:
		_fail("expected one mounted root, got %d" % get_child_count())
		return

	var mounted_root := get_child(0) as Control
	if mounted_root == null:
		_fail("mounted root is not a Control")
		return

	if not _validate_node_types(self):
		_fail("mounted tree contains an unexpected native node type")
		return

	var label := _find_sentinel(mounted_root)
	if label == null:
		_fail("sentinel Label was not mounted")
		return

	var styled_panel := _find_styled_panel(mounted_root)
	if styled_panel == null:
		_fail("styled View panel was not mounted")
		return

	if styled_panel.size.x <= 0.0 or styled_panel.size.y <= 0.0:
		_fail("styled View has an empty Yoga layout")
		return

	if label.size.x <= 0.0 or label.size.y <= 0.0:
		_fail("sentinel Text has an empty Yoga layout")
		return

	if not label.get_theme_color("font_color").is_equal_approx(EXPECTED_TEXT_COLOR):
		_fail("sentinel Text color does not match the fixture")
		return

	if label.get_theme_font_size("font_size") != EXPECTED_FONT_SIZE:
		_fail("sentinel Text font size does not match the fixture")
		return

	if not is_equal_approx(styled_panel.modulate.a, EXPECTED_OPACITY):
		_fail("styled View opacity does not match the fixture")
		return

	print("RN_BASELINE_OK")
	get_tree().quit(0)

func _process(_delta: float) -> void:
	frames += 1

	if settle_frames < 0:
		if get_child_count() > 0:
			settle_frames = 0
		elif frames >= TIMEOUT_FRAMES:
			_fail("no Fabric root commit after %d frames" % TIMEOUT_FRAMES)
		return

	settle_frames += 1
	if settle_frames >= SETTLE_FRAMES:
		set_process(false)
		_validate()
