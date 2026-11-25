extends Node

const WAIT_MSEC := 20000
const TEST_ID := "upstream-components"

var failed := false

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> bool:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)
	return false

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	return fail("timed out waiting for %s" % label)

func pause(seconds: float) -> void:
	await get_tree().create_timer(seconds).timeout

func node(native_id: String) -> Control:
	return root.get_native_node(native_id) as Control

func center(native_id: String) -> Vector2:
	return node(native_id).get_global_rect().get_center()

func mouse(at: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	event.position = at
	event.global_position = at
	if pressed:
		event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func drag(at: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	event.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(event)

func wheel_down(at: Vector2) -> void:
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_WHEEL_DOWN
		event.pressed = pressed
		event.factor = 1.0
		event.position = at
		event.global_position = at
		get_viewport().push_input(event)

func events() -> Array:
	return await js("__godotUpstream.events")

func clear_events() -> void:
	await js("__godotUpstream.events.length = 0")

func has_event(name: String) -> bool:
	return (await events()).has(name)

func background_of(native_id: String) -> Color:
	var style := node(native_id).get_theme_stylebox("panel") as RNStyleBox
	return Color.TRANSPARENT if style == null else style.get_bg_color()

func show_case(name: String, native_id: String) -> bool:
	await js("__godotUpstream.showCase('%s')" % name)
	if not await wait_until(func(): return node(native_id) != null, "the %s case to mount" % name):
		return false
	await pause(0.2)
	return true

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return node("opacity") != null, "the touchables to mount"):
		return
	await pause(0.2)
	for check in [check_opacity, check_disabled, check_highlight, check_without_feedback, check_virtualized_list, check_section_list, check_background, check_keyboard]:
		if not await check.call():
			return
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func check_opacity() -> bool:
	var opacity := node("opacity")
	await clear_events()
	mouse(center("opacity"), true)
	if not await wait_until(func(): return is_equal_approx(opacity.modulate.a, 0.4), "TouchableOpacity to reach activeOpacity"):
		return false
	mouse(center("opacity"), false)
	await get_tree().process_frame
	await get_tree().process_frame
	if opacity.modulate.a > 0.5:
		return fail("TouchableOpacity faded back before delayPressOut: %f" % opacity.modulate.a)
	if not await wait_until(func(): return is_equal_approx(opacity.modulate.a, 1.0) and await has_event("opacity:pressOut"), "TouchableOpacity to fade back after delayPressOut"):
		return false
	var short_press: Array = await events()
	if short_press != ["opacity:pressIn", "opacity:press", "opacity:pressOut"]:
		return fail("a short TouchableOpacity press fired %s" % [short_press])

	await clear_events()
	mouse(center("opacity"), true)
	if not await wait_until(func(): return await has_event("opacity:longPress"), "onLongPress"):
		return false
	mouse(center("opacity"), false)
	if not await wait_until(func(): return await has_event("opacity:pressOut"), "the long press to end"):
		return false
	var long_press: Array = await events()
	if long_press != ["opacity:pressIn", "opacity:longPress", "opacity:pressOut"]:
		return fail("a long TouchableOpacity press fired %s" % [long_press])
	return true

func check_disabled() -> bool:
	await clear_events()
	mouse(center("opacity_disabled"), true)
	await pause(0.1)
	mouse(center("opacity_disabled"), false)
	mouse(center("opacity_disabled"), true)
	await pause(0.7)
	mouse(center("opacity_disabled"), false)
	await pause(0.5)
	var fired: Array = await events()
	if not fired.is_empty():
		return fail("a disabled TouchableOpacity fired %s" % [fired])
	if not is_equal_approx(node("opacity_disabled").modulate.a, 1.0):
		return fail("a disabled TouchableOpacity faded to %f" % node("opacity_disabled").modulate.a)
	return true

func check_highlight() -> bool:
	await clear_events()
	var child := node("highlight_child")
	mouse(center("highlight"), true)
	if not await wait_until(func(): return await has_event("highlight:show"), "onShowUnderlay"):
		return false
	if not await wait_until(func(): return background_of("highlight").is_equal_approx(Color.RED), "the underlay colour"):
		return false
	if not is_equal_approx(child.modulate.a, 0.5):
		return fail("the TouchableHighlight child's opacity is %f while pressed" % child.modulate.a)
	mouse(center("highlight"), false)
	if not await wait_until(func(): return await has_event("highlight:hide"), "onHideUnderlay"):
		return false
	if not await has_event("highlight:press"):
		return fail("TouchableHighlight fired %s" % [await events()])
	if not await wait_until(func(): return is_equal_approx(child.modulate.a, 1.0) and not background_of("highlight").is_equal_approx(Color.RED), "the underlay to hide"):
		return false
	return true

func press_without_feedback(at: Vector2, release_at: Vector2) -> Array:
	await clear_events()
	mouse(at, true)
	await pause(0.1)
	drag(release_at)
	await pause(0.1)
	mouse(release_at, false)
	await pause(0.2)
	return await events()

func check_without_feedback() -> bool:
	# TouchableWithoutFeedback renders no view: its nativeID and responder
	# handlers are cloned onto the child.
	var child := node("without_feedback")
	if child == null:
		return fail("the TouchableWithoutFeedback child did not receive its nativeID")
	var rect := child.get_global_rect()
	var inside := rect.get_center()
	var fired: Array = await press_without_feedback(inside, inside)
	if fired != ["without:pressIn", "without:press"]:
		return fail("a press on the TouchableWithoutFeedback child fired %s" % [fired])
	var in_slop := Vector2(rect.position.x - 10, inside.y)
	fired = await press_without_feedback(in_slop, in_slop)
	if fired != ["without:pressIn", "without:press"]:
		return fail("a press inside hitSlop fired %s" % [fired])
	var past_slop := Vector2(rect.position.x - 30, inside.y)
	fired = await press_without_feedback(past_slop, past_slop)
	if not fired.is_empty():
		return fail("a press outside hitSlop fired %s" % [fired])
	fired = await press_without_feedback(inside, Vector2(rect.end.x + 60, inside.y))
	if fired != ["without:pressIn", "without:press"]:
		return fail("a release inside pressRetentionOffset fired %s" % [fired])
	fired = await press_without_feedback(inside, Vector2(rect.end.x + 120, inside.y))
	if fired != ["without:pressIn"]:
		return fail("a release outside pressRetentionOffset fired %s" % [fired])
	return true

func check_virtualized_list() -> bool:
	if not await show_case("lists", "virtualized"):
		return false
	if not await wait_until(func(): return int(await js("__godotUpstream.mounted.size")) >= 10, "VirtualizedList cells"):
		return false
	var at := center("virtualized")
	for i in 400:
		wheel_down(at)
		await get_tree().process_frame
		if await js("Math.max(...__godotUpstream.mounted) >= 200 && !__godotUpstream.mounted.has(0)"):
			break
	if not await js("Math.max(...__godotUpstream.mounted) >= 200 && !__godotUpstream.mounted.has(0)"):
		return fail("wheel scrolling did not move the VirtualizedList window: %s" % [await js("[...__godotUpstream.mounted]")])
	var most := int(await js("__godotUpstream.maxMounted"))
	if most <= 0 or most >= 100:
		return fail("VirtualizedList mounted %d of 1000 cells at once" % most)
	return true

func check_section_list() -> bool:
	if not await wait_until(func(): return node("header_section_5") != null, "the last section to render"):
		return false
	var order := []
	for section in 6:
		order.append("header_section_%d" % section)
		for item in 3:
			order.append("item_%d" % (section * 10 + item))
	var previous := -INF
	for native_id in order:
		var y := node(native_id).get_global_rect().position.y
		if y <= previous:
			return fail("%s renders above the entry before it" % native_id)
		previous = y
	var list_top := node("sections").get_global_rect().position.y
	await js("__godotUpstream.scrollSections(3)")
	if not await wait_until(func(): return is_equal_approx(node("header_section_3").get_global_rect().position.y, list_top), "scrollToLocation to reach section 3"):
		return false
	return true

func check_background() -> bool:
	if not await show_case("background", "background_image"):
		return false
	var image := node("background_image")
	var child := node("background_child")
	if not image is RNImageControl:
		return fail("ImageBackground's nativeID is on %s, not its Image" % image)
	if child.get_parent() != image.get_parent() or child.get_index() < image.get_index():
		return fail("ImageBackground's children do not render above its Image")
	if not is_equal_approx(image.modulate.a, 0.5):
		return fail("imageStyle opacity reached the Image as %f" % image.modulate.a)
	return true

func keyboard_layout() -> Array:
	return [node("kav_padding_child").size.y, node("kav_height").size.y, node("kav_position_child").get_global_rect().position.y]

func check_keyboard() -> bool:
	if not await show_case("keyboard", "kav_position_child"):
		return false
	var before := keyboard_layout()
	if before != [400.0, 400.0, 0.0]:
		return fail("KeyboardAvoidingView started at %s" % [before])
	# The keyboard covers the views' bottom 150 points.
	await js("__godotUpstream.showKeyboard(250)")
	if not await wait_until(func(): return keyboard_layout() == [250.0, 250.0, -150.0], "KeyboardAvoidingView to avoid the keyboard"):
		return false
	await js("__godotUpstream.hideKeyboard()")
	if not await wait_until(func(): return keyboard_layout() == before, "KeyboardAvoidingView to restore its layout"):
		return false
	return true
