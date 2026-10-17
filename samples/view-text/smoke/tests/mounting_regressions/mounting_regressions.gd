extends Control

@onready var surface: ReactNativeRootView = $Surface
var host: Control
var original_id: int

func frames() -> void:
	for i in 4:
		await get_tree().process_frame

func action(name: String) -> void:
	await ReactNativeHost.evaluate("__godotMountingRegression(%s, %d)" % [JSON.stringify(name), surface.get_root_tag()])
	await frames()

func events() -> Array:
	return await ReactNativeHost.evaluate("__godotMountingRegressionEvents")

func expect(condition: bool, message: String) -> bool:
	if not condition:
		printerr("RN_SMOKE_FAILED: mounting-regressions: %s" % message)
		get_tree().quit(1)
	return condition

func expect_opacity(value: float, message: String) -> bool:
	return expect(is_instance_valid(host) and host.get_instance_id() == original_id and is_equal_approx(host.modulate.a, value), "%s (opacity %s)" % [message, host.modulate.a if is_instance_valid(host) else -1.0])

func first_host() -> Control:
	return surface.get_child(0, true).get_child(0)

func click() -> void:
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.position = Vector2(20, 20)
		event.global_position = event.position
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		Input.parse_input_event(event)
	await frames()

func _ready() -> void:
	while surface.get_root_tag() == 0 or surface.get_child(0, true).get_child_count() == 0:
		await get_tree().process_frame
	await ReactNativeHost.evaluate(FileAccess.get_file_as_string("res://smoke/tests/mounting_regressions/mounting_regressions.js"))
	if not expect(ReactNativeHost.get_last_error().is_empty(), "fixture evaluation failed: %s" % ReactNativeHost.get_last_error()):
		return
	if not expect(await ReactNativeHost.evaluate("__godotInvalidShadowLookup"), "out-of-range shadow lookup did not return null"):
		return
	await action("setup")
	host = first_host()
	original_id = host.get_instance_id()
	await action("declarative")
	if not expect_opacity(0.7, "declarative update failed"):
		return
	await action("direct")
	if not expect_opacity(0.4, "direct override failed"):
		return
	await action("clone")
	if not expect_opacity(0.4, "clone commit erased an unchanged override"):
		return
	await action("coalesce")
	if not expect_opacity(0.6, "a later declarative change did not replace the override"):
		return
	await action("direct")
	await action("clear")
	if not expect_opacity(1.0, "clearing an override did not reset opacity to its default"):
		return
	await action("remove-leaf")
	if not expect(not is_instance_valid(host), "removed host remained mounted"):
		return
	await action("restore-leaf")
	host = first_host()
	if not expect(is_instance_valid(host) and host.get_instance_id() != original_id, "the restored leaf reused the removed host"):
		return
	for hidden_node in [surface, self, host]:
		hidden_node.hide()
		await action("clear-events")
		await click()
		if not expect((await events()).is_empty(), "hidden %s received input" % hidden_node.name):
			return
		hidden_node.show()
		await click()
		if not expect((await events()).has("topTouchEnd"), "shown %s did not receive input: %s" % [hidden_node.name, await events()]):
			return
	print("RN_SMOKE_OK: mounting-regressions")
	get_tree().quit(0)
