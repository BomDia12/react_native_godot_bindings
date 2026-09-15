extends ReactNativeRootView

const TIMEOUT_FRAMES := 1200
const INITIAL_COLOR := Color("335577")
const PRESSED_COLOR := Color("aa5500")
const CHANGED_COLOR := Color("227744")
const SECONDARY_COLOR := Color("773355")

var frames := 0
var stage := 0
var counter: Panel
var secondary: Panel
var counter_point := Vector2.ZERO
var secondary_point := Vector2.ZERO
var stop_log_start := 0
var count_before_cancel := 0

func fail(message: String) -> void:
	printerr("RN_SMOKE_FAILED: pressable-interaction: %s" % message)
	get_tree().quit(1)

func state() -> Dictionary:
	var value = HermesRuntime.get_global("__godotInteractionState")
	return value if value is Dictionary else {}

func event_log() -> Array:
	return state().get("log", [])

func has_log(name: String) -> bool:
	return name in event_log()

func find_panel_with_color(node: Node, color: Color) -> Panel:
	if node is Panel:
		var style := node.get_theme_stylebox("panel") as StyleBoxFlat
		if style != null and style.bg_color.is_equal_approx(color):
			return node
	for child in node.get_children():
		var found := find_panel_with_color(child, color)
		if found != null:
			return found
	return null

func border_width_of(panel: Panel) -> int:
	var style := panel.get_theme_stylebox("panel") as StyleBoxFlat
	return 0 if style == null else style.get_border_width(SIDE_TOP)

func find_label_text(node: Node, value: String) -> Label:
	if node is Label and node.text == value:
		return node
	for child in node.get_children():
		var found := find_label_text(child, value)
		if found != null:
			return found
	return null

func mouse_motion(position: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.device = 1
	event.position = position
	event.global_position = position
	get_viewport().push_input(event)

func mouse_button(position: Vector2, button: MouseButton, pressed: bool, canceled := false) -> void:
	var event := InputEventMouseButton.new()
	event.device = 1
	event.position = position
	event.global_position = position
	event.button_index = button
	event.pressed = pressed
	event.canceled = canceled
	get_viewport().push_input(event)

func key_event(keycode: Key, pressed: bool, echo := false) -> void:
	var event := InputEventKey.new()
	event.device = 1
	event.keycode = keycode
	event.physical_keycode = keycode
	event.pressed = pressed
	event.echo = echo
	get_viewport().push_input(event)

func touch_event(position: Vector2, pressed: bool, canceled := false) -> void:
	var event := InputEventScreenTouch.new()
	event.device = 1
	event.index = 7
	event.position = position
	event.pressed = pressed
	event.canceled = canceled
	get_viewport().push_input(event)

func assert_order(values: Array, expected: Array) -> bool:
	var cursor := 0
	for value in values:
		if cursor < expected.size() and value == expected[cursor]:
			cursor += 1
	return cursor == expected.size()

func finish() -> void:
	print("RN_SMOKE_OK: pressable-interaction")
	get_tree().quit(0)

func _process(_delta: float) -> void:
	frames += 1
	if frames > TIMEOUT_FRAMES:
		fail("timed out at stage %d with state %s" % [stage, state()])
		return

	var current := state()
	var log_values := event_log()
	match stage:
		0:
			if current.is_empty() or current.get("layouts", []).is_empty():
				return
			var priorities: Dictionary = current.get("priorityConstants", {})
			if int(priorities.get("default", -1)) != 0 or int(priorities.get("discrete", -1)) != 1 or int(priorities.get("continuous", -1)) != 2 or int(priorities.get("idle", -1)) != 3:
				fail("Fabric event priority constants are wrong: %s" % priorities)
				return
			if int(current.get("outsidePriority", -1)) != 0:
				fail("current event priority did not default outside dispatch")
				return
			counter = find_panel_with_color(self, INITIAL_COLOR)
			secondary = find_panel_with_color(self, SECONDARY_COLOR)
			if counter == null or secondary == null:
				return
			var layout: Dictionary = current.layouts[0]
			if not is_equal_approx(float(layout.width), 40.0) or not is_equal_approx(float(layout.height), 20.0):
				fail("initial onLayout rectangle is wrong: %s" % layout)
				return
			counter_point = counter.get_global_rect().get_center()
			secondary_point = secondary.get_global_rect().get_center()
			mouse_motion(counter_point)
			stage = 1
		1:
			if not has_log("hover-in"):
				return
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, true)
			stage = 2
		2:
			if not current.get("pressed", false):
				return
			counter = find_panel_with_color(self, PRESSED_COLOR)
			if counter == null:
				return
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, false)
			stage = 3
		3:
			if current.get("count", 0) != 1 or current.get("pressed", true) or current.get("layouts", []).size() < 2:
				return
			var changed_layout: Dictionary = current.layouts[1]
			if not is_equal_approx(float(changed_layout.width), 42.0) or not is_equal_approx(float(changed_layout.height), 20.0):
				fail("changed onLayout rectangle is wrong: %s" % changed_layout)
				return
			if find_label_text(self, "Count 1") == null:
				return
			var outlined := find_panel_with_color(self, CHANGED_COLOR)
			if outlined == null:
				return
			if border_width_of(outlined) != 2:
				fail("border props did not reach the stylebox")
				return
			if not assert_order(log_values, ["outer-capture", "inner-capture", "inner-bubble", "outer-bubble"]):
				fail("pointer capture and bubble order is wrong: %s" % log_values)
				return
			if not has_log("priority:pointer-down:1") or not has_log("priority:layout:0"):
				fail("event priorities were not visible inside handlers")
				return
			if not has_log("priority:pointer-move:2"):
				fail("continuous event priority was not visible inside a handler")
				return
			if not has_log("measure:2:2:40:20:2:2"):
				fail("measure did not return the six current-tree values")
				return
			if not assert_order(log_values, ["press", "render-count:1"]):
				fail("the event-triggered commit ran before the handler returned")
				return
			mouse_motion(secondary_point)
			mouse_button(secondary_point, MOUSE_BUTTON_LEFT, true)
			mouse_button(secondary_point, MOUSE_BUTTON_LEFT, false)
			stage = 4
		4:
			if not has_log("hover-out") or not has_log("secondary-focus"):
				return
			if not assert_order(log_values, ["counter-blur", "secondary-focus"]):
				fail("blur did not precede the next focus event: %s" % log_values)
				return
			HermesRuntime.set_global("__godotStopPropagation", true)
			stop_log_start = log_values.size()
			mouse_button(counter_point, MOUSE_BUTTON_RIGHT, true)
			stage = 5
		5:
			if event_log().size() < stop_log_start + 3:
				return
			var stopped := event_log().slice(stop_log_start)
			if "outer-bubble" in stopped:
				fail("stopPropagation allowed the outer bubble handler: %s" % stopped)
				return
			if not assert_order(stopped, ["outer-capture", "inner-capture", "inner-bubble"]):
				fail("stopped propagation order is wrong: %s" % stopped)
				return
			mouse_button(counter_point, MOUSE_BUTTON_RIGHT, false)
			HermesRuntime.set_global("__godotStopPropagation", false)
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, true)
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, false)
			stage = 6
		6:
			if current.get("count", 0) != 2 or not has_log("counter-focus"):
				return
			key_event(KEY_ENTER, true, true)
			key_event(KEY_ENTER, false, true)
			stage = 7
		7:
			if not has_log("key-down:Enter:Enter:true"):
				return
			if current.get("count", 0) != 2:
				fail("a repeated key activated the Pressable")
				return
			key_event(KEY_ENTER, true)
			key_event(KEY_ENTER, false)
			stage = 8
		8:
			if current.get("count", 0) != 3:
				return
			if not has_log("key-up:Enter:Enter:false"):
				fail("Enter key payload is incomplete")
				return
			key_event(KEY_SPACE, true)
			key_event(KEY_SPACE, false)
			stage = 9
		9:
			if current.get("count", 0) != 4:
				return
			touch_event(counter_point, true)
			touch_event(counter_point, false)
			stage = 10
		10:
			if current.get("count", 0) != 5:
				return
			if not has_log("touch-shape:1:1"):
				fail("touch arrays did not reach React")
				return
			count_before_cancel = current.count
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, true)
			stage = 11
		11:
			if not current.get("pressed", false):
				return
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, false, true)
			stage = 12
		12:
			if current.get("pressed", true):
				return
			if current.get("count", 0) != count_before_cancel:
				fail("canceled input invoked onPress")
				return
			if current.get("layouts", []).size() != 2:
				fail("unchanged commits emitted duplicate layout events: %s" % current.get("layouts", []))
				return
			ReactNativeFileSingleton.force_refresh()
			stage = 13
		13:
			if current.get("count", -1) != 0 or current.get("layouts", []).is_empty():
				return
			counter = find_panel_with_color(self, INITIAL_COLOR)
			if counter == null:
				return
			counter_point = counter.get_global_rect().get_center()
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, true)
			mouse_button(counter_point, MOUSE_BUTTON_LEFT, false)
			stage = 14
		14:
			if current.get("count", 0) != 1 or not has_log("counter-focus"):
				return
			var background := get_global_rect().get_center()
			mouse_button(background, MOUSE_BUTTON_LEFT, true)
			mouse_button(background, MOUSE_BUTTON_LEFT, false)
			stage = 15
		15:
			if not has_log("counter-blur"):
				return
			finish()
