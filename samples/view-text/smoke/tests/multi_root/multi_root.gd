extends Node

const TIMEOUT_FRAMES := 1800
const STRESS_CYCLES := 25

@onready var left: ReactNativeRootView = $Left
@onready var right: ReactNativeRootView = $Right

var frames := 0
var stage := 0
var stage_frame := 0
var initial_generation := 0
var old_left_tag := 0
var old_right_tag := 0
var right_native_id := 0
var timer_start := 0
var stale_handle = null
var seen_tags: Array[int] = []
var stress_cycle := 0
var stress_generation := 0
var declarative_override_requested := false

func snapshot() -> Dictionary:
	var value = HermesRuntime.get_global("__godotMultiRootSnapshot")
	return value if value is Dictionary else {}

func side(name: String) -> Dictionary:
	var value = snapshot().get(name, {})
	return value if value is Dictionary else {}

func call_fixture(function: String, name: String):
	return HermesRuntime.call_function(function, [name])

func target(node: Node) -> Panel:
	if node is Panel and is_equal_approx((node as Panel).size.y, 44.0):
		return node
	for child in node.get_children():
		var found := target(child)
		if found != null:
			return found
	return null

func click(root: ReactNativeRootView) -> void:
	var panel := target(root)
	if panel == null:
		return
	var point := panel.get_global_rect().get_center()
	var down := InputEventMouseButton.new()
	down.position = point
	down.global_position = point
	down.button_index = MOUSE_BUTTON_LEFT
	down.pressed = true
	get_viewport().push_input(down)
	var up := InputEventMouseButton.new()
	up.position = point
	up.global_position = point
	up.button_index = MOUSE_BUTTON_LEFT
	up.pressed = false
	get_viewport().push_input(up)

func fail(message: String) -> void:
	printerr("RN_SMOKE_FAILED: multi-root: %s" % message)
	get_tree().quit(1)

func advance() -> void:
	stage += 1
	stage_frame = frames

func remember_tag(tag: int) -> bool:
	if tag in seen_tags:
		return false
	seen_tags.append(tag)
	return true

func _process(_delta: float) -> void:
	frames += 1
	if frames > TIMEOUT_FRAMES:
		fail("timed out at stage %d with state %s" % [stage, snapshot()])
		return

	match stage:
		0:
			if side("left").get("count", -1) != 0 or side("right").get("count", -1) != 0:
				return
			old_left_tag = left.get_root_tag()
			old_right_tag = right.get_root_tag()
			if old_left_tag < 11 or old_right_tag != old_left_tag + 10:
				fail("root tags did not use the process-wide +10 sequence")
				return
			if not remember_tag(old_left_tag) or not remember_tag(old_right_tag):
				fail("initial root tags were reused")
				return
			initial_generation = HermesRuntime.get_runtime_generation()
			var probe = call_fixture("__godotMultiRootProbe", "left")
			if not probe is Dictionary:
				return
			if probe.tagName != "RN:RCTView" or probe.textContent != "left:0":
				fail("public View ref identity or text content is wrong: %s" % probe)
				return
			if not probe.connected or not probe.documentConnected or not probe.parentMatches or not probe.byIdMatches or not probe.documentElementParentMatches:
				fail("public ref traversal is disconnected: %s" % probe)
				return
			if probe.childCount != 1 or probe.position != 20 or probe.samePosition != 0 or probe.documentPosition != 0 or probe.crossPosition != 1 or probe.followingPosition != 4 or probe.precedingPosition != 2 or probe.nullHandle != null or probe.numericHandle != 123:
				fail("public ref traversal contract is wrong: %s" % probe)
				return
			if probe.compositeHandle != probe.handle:
				fail("findNodeHandle did not resolve a composite ref: %s" % probe)
				return
			if probe.measure == null or probe.windowMeasure == null or probe.layoutMeasure == null or probe.layoutMeasure is String:
				fail("public ref geometry did not resolve: %s" % probe)
				return
			call_fixture("__godotMultiRootBatchedIncrement", "right")
			advance()
		1:
			if side("right").get("count", 0) != 2:
				return
			if side("right").get("renders", 0) != 2:
				fail("unstable_batchedUpdates produced more than one render")
				return
			right_native_id = right.get_child(0).get_instance_id()
			call_fixture("__godotMultiRootSetProps", "left")
			call_fixture("__godotMultiRootFocus", "left")
			advance()
		2:
			var left_target := target(left)
			if left_target == null or not is_equal_approx(left_target.modulate.a, 0.4) or not is_equal_approx(left_target.size.x, 100.0):
				if frames - stage_frame > 30:
					fail("setNativeProps did not update layout and opacity: width=%s opacity=%s events=%s" % [left_target.size.x if left_target != null else -1, left_target.modulate.a if left_target != null else -1, side("left").get("events", [])])
				return
			if "layout:100" not in side("left").get("events", []):
				if frames - stage_frame > 30:
					fail("layout-affecting setNativeProps did not emit onLayout")
				return
			if "focus" not in side("left").get("events", []):
				if frames - stage_frame > 30:
					fail("public focus did not emit focus: %s" % [side("left").get("events", [])])
				return
			call_fixture("__godotMultiRootBlur", "left")
			click(left)
			advance()
		3:
			var events: Array = side("left").get("events", [])
			if side("left").get("count", 0) != 1 or "blur" not in events:
				return
			if "capture-pending:true" not in events or "got-capture" not in events or "lost-capture" not in events:
				fail("pointer capture transitions are incomplete: %s" % events)
				return
			var left_target := target(left)
			if not declarative_override_requested:
				if left_target == null or not is_equal_approx(left_target.modulate.a, 0.4):
					fail("an unrelated commit cleared direct props")
					return
				call_fixture("__godotMultiRootSetDeclarativeOpacity", "left")
				declarative_override_requested = true
				return
			if left_target == null or not is_equal_approx(left_target.modulate.a, 0.7) or not is_equal_approx(left_target.size.x, 95.0):
				if frames - stage_frame > 30:
					fail("declarative props did not replace direct props: width=%s opacity=%s" % [left_target.size.x if left_target != null else -1, left_target.modulate.a if left_target != null else -1])
				return
			stale_handle = call_fixture("__godotMultiRootSaveRef", "left")
			left.reload()
			advance()
		4:
			if side("left").get("count", -1) != 0:
				return
			if call_fixture("__godotMultiRootStaleHandle", "left") != stale_handle:
				fail("findNodeHandle did not preserve the pinned numeric tag for an unmounted public ref")
				return
			var stale_probe = call_fixture("__godotMultiRootStaleProbe", "left")
			if not stale_probe is Dictionary or stale_probe.connected or stale_probe.tagName != "" or stale_probe.textContent != "" or stale_probe.childCount != 0 or not stale_probe.parentMissing or not stale_probe.zeroBounds or not stale_probe.measureSkipped:
				fail("an unmounted public ref did not return neutral values: %s" % stale_probe)
				return
			if left.get_root_tag() == old_left_tag or not remember_tag(left.get_root_tag()):
				fail("soft reload reused a root tag")
				return
			if side("right").get("count", 0) != 2 or right.get_child(0).get_instance_id() != right_native_id:
				fail("soft reload changed the peer root")
				return
			left.application_key = "MissingApplication"
			advance()
		5:
			if left.get_child_count() != 0:
				return
			if side("right").get("count", 0) != 2 or right.get_child(0).get_instance_id() != right_native_id:
				fail("an invalid key changed the peer root")
				return
			if not remember_tag(left.get_root_tag()):
				fail("invalid-key restart reused a root tag")
				return
			left.application_key = "GodotLeftApp"
			advance()
		6:
			if side("left").get("count", -1) != 0 or left.get_child_count() == 0:
				return
			if not remember_tag(left.get_root_tag()):
				fail("corrected key reused a root tag")
				return
			remove_child(left)
			advance()
		7:
			if side("right").get("count", 0) != 2 or right.get_child(0).get_instance_id() != right_native_id:
				fail("removing one root changed its peer")
				return
			add_child(left)
			advance()
		8:
			if side("left").get("count", -1) != 0 or left.get_child_count() == 0:
				return
			if not remember_tag(left.get_root_tag()):
				fail("scene re-entry reused a root tag")
				return
			ReactNativeFileSingleton.force_refresh()
			advance()
		9:
			if HermesRuntime.get_runtime_generation() != initial_generation + 1:
				return
			if side("left").get("count", -1) != 0 or side("right").get("count", -1) != 0:
				return
			if not remember_tag(left.get_root_tag()) or not remember_tag(right.get_root_tag()):
				fail("bundle refresh reused a root tag")
				return
			left.application_key = "GodotBrokenApp"
			advance()
		10:
			var boundary_caught = HermesRuntime.get_global("__godotBoundaryCaught")
			if boundary_caught != true:
				return
			if target(left) != null:
				fail("component error left failed surface content mounted")
				return
			if side("right").get("count", -1) != 0 or right.get_child_count() == 0:
				fail("component error escaped its surface boundary")
				return
			left.application_key = "GodotLeftApp"
			advance()
		11:
			if side("left").get("count", -1) != 0 or left.get_child_count() == 0:
				return
			timer_start = int(HermesRuntime.get_global("__godotMultiTimerTicks"))
			advance()
		12:
			if frames - stage_frame < 12:
				return
			var timer_delta := int(HermesRuntime.get_global("__godotMultiTimerTicks")) - timer_start
			if timer_delta < 10 or timer_delta > 14:
				fail("timers drained %d times in 12 frames with two roots" % timer_delta)
				return
			left.reload()
			advance()
		13:
			if target(left) == null:
				return
			if not remember_tag(left.get_root_tag()):
				fail("stress soft reload reused a root tag")
				return
			left.application_key = "MissingApplication"
			advance()
		14:
			if left.get_child_count() != 0:
				return
			if not remember_tag(left.get_root_tag()):
				fail("stress failure reused a root tag")
				return
			left.application_key = "GodotLeftApp"
			advance()
		15:
			if target(left) == null:
				return
			if not remember_tag(left.get_root_tag()):
				fail("stress recovery reused a root tag")
				return
			remove_child(left)
			advance()
		16:
			if left.get_root_tag() != 0 or left.get_child_count() != 0:
				return
			if target(right) == null:
				fail("stress removal changed the peer root")
				return
			add_child(left)
			advance()
		17:
			if target(left) == null:
				return
			if not remember_tag(left.get_root_tag()):
				fail("stress scene re-entry reused a root tag")
				return
			stress_generation = HermesRuntime.get_runtime_generation()
			ReactNativeFileSingleton.force_refresh()
			advance()
		18:
			if HermesRuntime.get_runtime_generation() != stress_generation + 1 or target(left) == null or target(right) == null:
				return
			if not remember_tag(left.get_root_tag()) or not remember_tag(right.get_root_tag()):
				fail("stress refresh reused a root tag")
				return
			stress_cycle += 1
			if stress_cycle < STRESS_CYCLES:
				left.reload()
				stage = 13
				stage_frame = frames
				return
			print("RN_SMOKE_OK: multi-root")
			get_tree().quit(0)
