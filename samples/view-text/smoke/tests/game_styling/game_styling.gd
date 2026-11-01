extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-styling"

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

func panel_of(native_id: String) -> StyleBox:
	return (root.get_native_node(native_id) as Control).get_theme_stylebox("panel")

func _run() -> void:
	if not await wait_until(func(): return root.get_native_node("shaded") != null and (await js("__godotStyling.layouts.title != null && __godotStyling.layouts.plain != null")), "the views to mount"):
		return

	var themed: Control = root.get_native_node("themed")
	if themed.theme_type_variation != &"FramedPanel" or (panel_of("themed") as StyleBoxFlat) == null or not (panel_of("themed") as StyleBoxFlat).bg_color.is_equal_approx(Color(0.2, 0.4, 0.6)):
		fail("the theme type variation's panel does not paint the view")
		return
	if not await wait_until(func(): return panel_of("framed") == load("res://smoke/tests/game_styling/frame.tres"), "the StyleBox to load"):
		return
	if (root.get_native_node("outlined") as Control).get_theme_stylebox("panel") is StyleBoxFlat == false:
		fail("a skinned view with an outline lost its skin")
		return
	var painters: Array[Node] = root.get_native_node("themed_filtered").find_children("*", "RNViewControl", true, false)
	if painters.is_empty() or not ((painters[0] as Control).get_theme_stylebox("panel") as StyleBoxFlat).bg_color.is_equal_approx(Color(0.2, 0.4, 0.6)):
		fail("a theme type does not reach the painter inside an effect host")
		return
	if (root.get_native_node("tooltip") as Control).get_tooltip(Vector2.ZERO) != "Restores 50 HP":
		fail("the tooltip prop did not reach the control")
		return

	var title_height: float = await js("__godotStyling.layouts.title.height")
	var plain_height: float = await js("__godotStyling.layouts.plain.height")
	var sized_height: float = await js("__godotStyling.layouts.sized.height")
	if title_height < plain_height * 2.0:
		fail("the Text theme type's font size did not apply: %s against %s" % [title_height, plain_height])
		return
	if sized_height >= title_height / 2.0:
		fail("an explicit fontSize lost to the Text theme type: %s against %s" % [sized_height, title_height])
		return

	if not await wait_until(func(): return shader_material("shaded") != null, "the material stage"):
		return
	var shaded := shader_material("shaded")
	var other := shader_material("shaded_other")
	if shaded == other or shaded == load("res://smoke/tests/game_styling/tint.tres"):
		fail("views share one material instance")
		return
	if not is_equal_approx(shaded.get_shader_parameter("strength"), 0.25) or not (shaded.get_shader_parameter("tint") as Color).is_equal_approx(Color(1, 0, 0, 1)):
		fail("the shader parameters did not apply: %s %s" % [shaded.get_shader_parameter("strength"), shaded.get_shader_parameter("tint")])
		return
	if not is_equal_approx(other.get_shader_parameter("strength"), 0.75):
		fail("the second view's shader parameters did not apply")
		return
	await js("__godotStyling.setStrength(0.5)")
	if not await wait_until(func(): return is_equal_approx(shader_material("shaded").get_shader_parameter("strength"), 0.5), "the new shader parameter"):
		return
	# The shaded children stay reachable through the stage.
	if not await js("__godotStyling.layouts.inner != null"):
		fail("the shaded view's child did not lay out")
		return

	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func shader_material(native_id: String) -> ShaderMaterial:
	var pending: Array[Node] = [root.get_native_node(native_id)]
	while not pending.is_empty():
		var node: Node = pending.pop_back()
		if node is SubViewportContainer and node.material is ShaderMaterial and node.material.shader.resource_path.ends_with("tint.gdshader"):
			return node.material
		pending.append_array(node.get_children(true))
	return null
