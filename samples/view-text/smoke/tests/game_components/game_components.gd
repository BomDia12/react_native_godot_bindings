extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-components"

var failed := false
var pointer_position := Vector2(-1, -1)

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
	$Minimap/Pointer.gui_input.connect(func(event: InputEvent):
		if event is InputEventMouseButton:
			pointer_position = event.position)
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return root.get_native_node("bar") != null and (await js("__godotComponents.layouts.rich != null")), "the components to mount"):
		return
	if not await _check_progress():
		return
	if not await _check_rich_text():
		return
	if not await _check_viewport():
		return
	if not await _check_video():
		return
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func _check_progress() -> bool:
	var bar: ProgressBar = root.get_native_node("bar")
	if not is_equal_approx(bar.value, 30.0) or not is_equal_approx(bar.max_value, 60.0) or bar.fill_mode != ProgressBar.FILL_END_TO_BEGIN:
		fail("the progress bar props did not apply: %s/%s mode %s" % [bar.value, bar.max_value, bar.fill_mode])
		return false
	var themed_height: float = await js("__godotComponents.layouts.themedBar.height")
	var plain_height: float = await js("__godotComponents.layouts.bar.height")
	if themed_height <= plain_height:
		fail("the theme type's minimum size did not reach layout: %s against %s" % [themed_height, plain_height])
		return false
	await js("__godotComponents.setValue(45)")
	if not await wait_until(func(): return is_equal_approx(bar.value, 45.0), "the progress value to update"):
		return false
	var ring: TextureProgressBar = root.get_native_node("ring")
	if ring.fill_mode != TextureProgressBar.FILL_CLOCKWISE or not is_equal_approx(ring.radial_fill_degrees, 180.0):
		fail("the texture progress bar props did not apply")
		return false
	if not await wait_until(func(): return ring.texture_progress != null and ring.texture_under != null, "the progress textures to load"):
		return false
	return true

func _check_rich_text() -> bool:
	var label: RichTextLabel = root.get_native_node("rich")
	var layout: Dictionary = await js("__godotComponents.layouts.rich")
	if not is_equal_approx(label.size.x, layout.width) or label.get_content_height() > layout.height + 1.0:
		fail("the rich text's measured size %s does not fit its label's content %s" % [layout, label.get_content_height()])
		return false
	var short_height: float = await js("__godotComponents.layouts.richShort.height")
	if layout.height <= short_height:
		fail("long BBCode did not wrap to a taller layout: %s against %s" % [layout.height, short_height])
		return false
	var narrow_height: float = await js("__godotComponents.layouts.richNarrow.height")
	var wide_height: float = await js("__godotComponents.layouts.richWide.height")
	if narrow_height <= wide_height:
		fail("one BBCode text did not wrap to each width: %s at 80 against %s at 400" % [narrow_height, wide_height])
		return false
	if not (root.get_native_node("rich") as RichTextLabel).get_parsed_text().contains("wares"):
		fail("the BBCode did not parse")
		return false
	if label.custom_effects.size() != 1 or label.get_parsed_text().contains("[smoke]"):
		fail("the custom effect's tag did not parse: %s" % label.get_parsed_text())
		return false
	await js("__godotComponents.reveal()")
	if not await wait_until(func(): return await js("__godotComponents.revealed === 1"), "the reveal to finish"):
		return false
	if not is_equal_approx(label.visible_ratio, 1.0):
		fail("the reveal did not finish at its ratio")
		return false
	label.meta_clicked.emit("shop")
	if not await wait_until(func(): return await js("__godotComponents.meta === 'shop'"), "the meta click event"):
		return false
	return true

func _check_viewport() -> bool:
	var view: TextureRect = root.get_native_node("minimap")
	if not await wait_until(func(): return await js("__godotComponents.viewportErrors.length === 1"), "the missing viewport's error"):
		return false
	if view.texture != null:
		fail("a missing viewport shows a texture")
		return false
	ReactNativeHost.register_viewport("minimap", $Minimap)
	if not await wait_until(func(): return view.texture == $Minimap.get_texture() and await js("__godotComponents.viewportLoads === 1"), "the viewport to show"):
		return false
	if $Minimap.size != Vector2i(view.size.ceil()):
		fail("matchSize did not size the viewport: %s against %s" % [$Minimap.size, view.size])
		return false
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	click.position = view.get_global_rect().position + Vector2(10, 20)
	click.global_position = click.position
	get_viewport().push_input(click)
	if not await wait_until(func(): return pointer_position != Vector2(-1, -1), "the forwarded click"):
		return false
	if not pointer_position.is_equal_approx(Vector2(10, 20)):
		fail("the forwarded click landed at %s" % pointer_position)
		return false
	ReactNativeHost.unregister_viewport("minimap")
	if not await wait_until(func(): return view.texture == null, "the viewport to clear"):
		return false
	return true

func _check_video() -> bool:
	var video: VideoStreamPlayer = root.get_native_node("video")
	if not await wait_until(func(): return video.stream != null and await js("__godotComponents.videoLoads === 1"), "the video to load"):
		return false
	if not video.is_playing() and not await js("__godotComponents.videoEnds > 0"):
		fail("autoplay did not start the video")
		return false
	if not await wait_until(func(): return await js("__godotComponents.videoEnds > 0"), "the video to end"):
		return false
	return true
