extends Node

const WAIT_MSEC := 20000
const STRESS_CYCLES := 25

@onready var left: ReactNativeRootView = $Left
@onready var right: ReactNativeRootView = $Right

var seen_tags: Array[int] = []
var failed := false

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: multi-root: %s" % message)
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func side(name: String) -> Dictionary:
	var value = await js("__godotMultiRootSnapshot[%s]" % JSON.stringify(name))
	return value if value is Dictionary else {}

func call_fixture(function: String, name: String) -> Variant:
	return await js("%s(%s)" % [function, JSON.stringify(name)])

func count(name: String) -> int:
	return int((await side(name)).get("count", -1))

func events(name: String) -> Array:
	return (await side(name)).get("events", [])

func target(root: ReactNativeRootView) -> Panel:
	return find_target(root.get_child(0, true))

func find_target(node: Node) -> Panel:
	if node is Panel and is_equal_approx((node as Panel).size.y, 44.0):
		return node
	for child in node.get_children():
		var found := find_target(child)
		if found != null:
			return found
	return null

# Polls an asynchronous condition until it holds, failing after WAIT_MSEC.
func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func click(root: ReactNativeRootView) -> void:
	var panel := target(root)
	if panel == null:
		return
	var point := panel.get_global_rect().get_center()
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.position = point
		event.global_position = point
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		get_viewport().push_input(event)

func remember_tag(tag: int, label: String) -> bool:
	if tag in seen_tags:
		fail("%s reused a root tag" % label)
		return false
	seen_tags.append(tag)
	return true

func opacity_and_width(opacity: float, width: float) -> bool:
	var panel := target(left)
	return panel != null and is_equal_approx(panel.modulate.a, opacity) and is_equal_approx(panel.size.x, width)

func peer_unchanged(right_native_id: int) -> bool:
	return await count("right") == 2 and target(right) != null and target(right).get_instance_id() == right_native_id

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return await count("left") == 0 and await count("right") == 0, "both roots to mount"):
		return
	if left.get_root_tag() < 1 or right.get_root_tag() != left.get_root_tag() + 10:
		fail("root tags did not use the process-wide +10 sequence")
		return
	if not remember_tag(left.get_root_tag(), "the initial left root") or not remember_tag(right.get_root_tag(), "the initial right root"):
		return
	var initial_generation := ReactNativeHost.get_generation()

	var probe = await call_fixture("__godotMultiRootProbe", "left")
	if not probe is Dictionary:
		fail("public ref probe did not resolve")
		return
	if probe.textContent != "left:0" or not probe.connected or not probe.documentConnected or not probe.parentMatches or not probe.byIdMatches or not probe.documentElementParentMatches:
		fail("public ref traversal is wrong: %s" % [probe])
		return
	if probe.childCount != 1 or probe.position != 20 or probe.samePosition != 0 or probe.documentPosition != 0 or probe.crossPosition != 1 or probe.followingPosition != 4 or probe.precedingPosition != 2 or probe.nullHandle != null or probe.numericHandle != 123:
		fail("public ref traversal contract is wrong: %s" % [probe])
		return
	if probe.compositeHandle != probe.handle:
		fail("findNodeHandle did not resolve a composite ref: %s" % [probe])
		return
	if probe.measure == null or probe.windowMeasure == null or probe.layoutMeasure == null or probe.layoutMeasure is String:
		fail("public ref geometry did not resolve: %s" % [probe])
		return

	await call_fixture("__godotMultiRootBatchedIncrement", "right")
	if not await wait_until(func(): return await count("right") == 2, "the batched increment"):
		return
	if int((await side("right")).get("renders", 0)) != 2:
		fail("unstable_batchedUpdates produced more than one render")
		return
	var right_native_id := target(right).get_instance_id()

	await call_fixture("__godotMultiRootSetProps", "left")
	await call_fixture("__godotMultiRootFocus", "left")
	if not await wait_until(func(): return opacity_and_width(0.4, 101.0), "setNativeProps opacity and width"):
		return
	if not await wait_until(func():
		var seen := await events("left")
		return "layout:101" in seen and "focus" in seen, "the setNativeProps layout and focus"):
		return

	await call_fixture("__godotMultiRootBlur", "left")
	click(left)
	if not await wait_until(func(): return await count("left") == 1 and "blur" in await events("left"), "the click and blur"):
		return
	var seen_events := await events("left")
	if "capture-pending:true" not in seen_events or "got-capture" not in seen_events or "lost-capture" not in seen_events:
		fail("pointer capture transitions are incomplete: %s" % [seen_events])
		return
	if not opacity_and_width(0.4, 101.0):
		fail("an unrelated commit cleared direct props")
		return
	await call_fixture("__godotMultiRootSetDeclarativeOpacity", "left")
	if not await wait_until(func(): return opacity_and_width(0.7, 95.0), "declarative props replacing direct props"):
		return
	await call_fixture("__godotMultiRootSetTemporaryProps", "left")
	if not await wait_until(func(): return opacity_and_width(0.3, 80.0), "temporary direct props"):
		return

	var stale_handle = await call_fixture("__godotMultiRootSaveRef", "left")
	var old_left_tag := left.get_root_tag()
	left.reload()
	if not await wait_until(func(): return left.get_root_tag() != old_left_tag and await count("left") == 0 and target(left) != null, "the soft reload"):
		return
	if await call_fixture("__godotMultiRootStaleHandle", "left") != stale_handle:
		fail("findNodeHandle did not preserve the pinned numeric tag for an unmounted public ref")
		return
	var stale_probe = await call_fixture("__godotMultiRootStaleProbe", "left")
	if not stale_probe is Dictionary or stale_probe.connected or stale_probe.textContent != "" or stale_probe.childCount != 0 or not stale_probe.parentMissing or not stale_probe.zeroBounds or not stale_probe.measureSkipped:
		fail("an unmounted public ref did not return neutral values: %s" % [stale_probe])
		return
	if not remember_tag(left.get_root_tag(), "soft reload"):
		return
	if not await peer_unchanged(right_native_id):
		fail("soft reload changed the peer root")
		return

	left.application_key = "MissingApplication"
	if not await wait_until(func(): return target(left) == null, "the invalid key to unmount"):
		return
	if not await peer_unchanged(right_native_id):
		fail("an invalid key changed the peer root")
		return
	if not remember_tag(left.get_root_tag(), "the invalid-key restart"):
		return
	left.application_key = "GodotLeftApp"
	if not await wait_until(func(): return target(left) != null and await count("left") == 0, "the corrected key"):
		return
	if not remember_tag(left.get_root_tag(), "the corrected key"):
		return

	remove_child(left)
	await get_tree().process_frame
	if not await peer_unchanged(right_native_id):
		fail("removing one root changed its peer")
		return
	add_child(left)
	if not await wait_until(func(): return target(left) != null and await count("left") == 0, "scene re-entry"):
		return
	if not remember_tag(left.get_root_tag(), "scene re-entry"):
		return

	ReactNativeFileSingleton.force_refresh()
	if not await wait_until(func(): return ReactNativeHost.get_generation() == initial_generation + 1 and await count("left") == 0 and await count("right") == 0 and target(left) != null and target(right) != null, "the bundle refresh"):
		return
	if not remember_tag(left.get_root_tag(), "the bundle refresh") or not remember_tag(right.get_root_tag(), "the bundle refresh"):
		return

	left.application_key = "GodotBrokenApp"
	if not await wait_until(func(): return await js("__godotBoundaryCaught") == true and target(left) == null, "the surface error boundary"):
		return
	if await count("right") != 0 or target(right) == null:
		fail("component error escaped its surface boundary")
		return
	left.application_key = "GodotLeftApp"
	if not await wait_until(func(): return target(left) != null, "recovery from the component error"):
		return
	var ticks := int(await js("__godotMultiTimerTicks"))
	await get_tree().create_timer(0.2).timeout
	if int(await js("__godotMultiTimerTicks")) <= ticks:
		fail("JS timers stopped while two roots were running")
		return

	var dev_generation := ReactNativeHost.get_generation()
	await js("nativeModuleProxy.DevSettings.reloadWithReason('smoke')")
	if not await wait_until(func(): return ReactNativeHost.get_generation() == dev_generation + 1 and target(left) != null and target(right) != null, "the DevSettings reload"):
		return
	await get_tree().create_timer(0.2).timeout
	if ReactNativeHost.get_generation() != dev_generation + 1:
		fail("the DevSettings reload replaced the runtime more than once")
		return
	if not remember_tag(left.get_root_tag(), "the DevSettings reload") or not remember_tag(right.get_root_tag(), "the DevSettings reload"):
		return

	var bundle := ReactNativeFileSingleton.get_monitored_file()
	ReactNativeFileSingleton.set_monitored_file("res://smoke/tests/multi_root/missing.bundle.js")
	if not await wait_until(func(): return left.get_root_tag() == 0 and right.get_root_tag() == 0 and target(left) == null and target(right) == null, "the roots to stop without a bundle"):
		return
	ReactNativeFileSingleton.set_monitored_file(bundle)
	if not await wait_until(func(): return target(left) != null and target(right) != null and await count("left") == 0 and await count("right") == 0, "the late bundle"):
		return
	if not remember_tag(left.get_root_tag(), "the late bundle") or not remember_tag(right.get_root_tag(), "the late bundle"):
		return

	for cycle in STRESS_CYCLES:
		var reloaded_tag := left.get_root_tag()
		left.reload()
		if not await wait_until(func(): return target(left) != null and left.get_root_tag() != reloaded_tag, "a stress reload"):
			return
		if not remember_tag(left.get_root_tag(), "a stress reload"):
			return
		left.application_key = "MissingApplication"
		if not await wait_until(func(): return target(left) == null, "a stress failure"):
			return
		if not remember_tag(left.get_root_tag(), "a stress failure"):
			return
		left.application_key = "GodotLeftApp"
		if not await wait_until(func(): return target(left) != null, "a stress recovery"):
			return
		if not remember_tag(left.get_root_tag(), "a stress recovery"):
			return
		remove_child(left)
		if left.get_root_tag() != 0 or target(left) != null or target(right) == null:
			fail("stress removal did not stop only the removed root")
			return
		add_child(left)
		if not await wait_until(func(): return target(left) != null, "a stress re-entry"):
			return
		if not remember_tag(left.get_root_tag(), "a stress re-entry"):
			return
		var generation := ReactNativeHost.get_generation()
		ReactNativeFileSingleton.force_refresh()
		if not await wait_until(func(): return ReactNativeHost.get_generation() == generation + 1 and target(left) != null and target(right) != null, "a stress refresh"):
			return
		if not remember_tag(left.get_root_tag(), "a stress refresh") or not remember_tag(right.get_root_tag(), "a stress refresh"):
			return
	print("RN_SMOKE_OK: multi-root")
	get_tree().quit(0)
