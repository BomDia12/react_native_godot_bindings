extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-slider"

var failed := false

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func _ready() -> void:
	_run.call_deferred()

func mouse(at: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	event.position = at
	event.global_position = at
	get_viewport().push_input(event)

func drag_to(at: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func slider_of(native_id: String) -> Slider:
	return (root.get_native_node(native_id) as Control).get_child(0, true)

func _run() -> void:
	if not await wait_until(func(): return root.get_native_node("slider") != null and (await js("__godotSlider.layouts.unsized != null")), "the sliders to mount"):
		return
	var slider := slider_of("slider")
	if not slider is HSlider or not is_equal_approx(slider.value, 0.25) or not is_equal_approx(slider.step, 0.05):
		fail("the slider props did not apply: %s step %s" % [slider.value, slider.step])
		return
	var vertical := slider_of("vertical")
	if not vertical is VSlider or not is_equal_approx(vertical.value, 8.0):
		fail("the inverted vertical slider did not mirror its value: %s" % vertical.value)
		return
	var unsized: Dictionary = await js("__godotSlider.layouts.unsized")
	if unsized.height <= 0.0:
		fail("a slider without a size has no theme height")
		return
	await js("__godotSlider.setValue(0.5)")
	if not await wait_until(func(): return is_equal_approx(slider.value, 0.5), "the controlled value"):
		return
	if await js("__godotSlider.events.length") != 0:
		fail("a controlled value emitted events")
		return
	var skinned := slider_of("skinned")
	if not await wait_until(func(): return skinned.has_theme_icon_override("grabber"), "the thumb image"):
		return
	if not skinned.has_theme_stylebox_override("grabber_area"):
		fail("the minimum track color did not apply")
		return

	var rect := slider.get_global_rect()
	var y := rect.get_center().y
	mouse(Vector2(rect.position.x + rect.size.x * 0.5, y), true)
	await get_tree().process_frame
	drag_to(Vector2(rect.end.x, y))
	await get_tree().process_frame
	mouse(Vector2(rect.end.x, y), false)
	if not await wait_until(func(): return await js("__godotSlider.events.some(event => event[0] === 'complete')"), "the drag to complete"):
		return
	var events: Array = await js("__godotSlider.events")
	var names := events.map(func(event): return event[0])
	if names.find("start") == -1 or names.find("change") == -1 or names.find("start") > names.find("complete"):
		fail("the slider's events came out of order: %s" % [events])
		return
	if not is_equal_approx(slider.value, 0.8):
		fail("upperLimit did not stop the drag: %s" % slider.value)
		return
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)
