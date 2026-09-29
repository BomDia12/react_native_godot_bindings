extends Control

@onready var surface: ReactNativeRootView = $Surface
var host: Control
var original_id: int

func frames() -> void:
	for i in 4:
		await get_tree().process_frame

func action(name: String) -> void:
	HermesRuntime.call_function("__godotMountingRegression", [name, surface.get_root_tag()])

func expect(condition: bool, message: String) -> bool:
	if not condition:
		printerr("RN_SMOKE_FAILED: mounting-regressions: %s" % message)
		get_tree().quit(1)
	return condition

func expect_opacity(value: float, message: String) -> bool:
	return expect(is_instance_valid(host) and host.get_instance_id() == original_id and is_equal_approx(host.modulate.a, value), message)

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
	await frames()
	HermesRuntime.evaluate(FileAccess.get_file_as_string("res://smoke/tests/mounting_regressions/mounting_regressions.js"), "mounting-regressions.js")
	if not expect(HermesRuntime.get_last_error().is_empty(), "fixture evaluation failed"):
		return
	action("setup")
	await frames()
	host = surface.get_node("ReactNativeMountContainer").get_child(0)
	original_id = host.get_instance_id()
	action("declarative")
	await frames()
	if not expect_opacity(0.7, "declarative update failed"):
		return
	action("direct")
	await frames()
	if not expect_opacity(0.4, "direct override failed"):
		return
	for operation in ["sibling", "clone"]:
		action(operation)
		await frames()
		if not expect_opacity(0.4, "%s commit erased an unchanged override" % operation):
			return
	action("coalesce")
	await frames()
	if not expect_opacity(0.6, "coalescing lost a pending declarative prop change"):
		return
	action("direct")
	await frames()
	action("roundtrip")
	await frames()
	if not expect_opacity(0.6, "coalescing lost a prop change back to its published value"):
		return
	action("direct")
	await frames()
	surface.set_mount_failure_injection(-1, 0)
	action("reject")
	await frames()
	if not expect_opacity(0.4, "rejection changed the published override"):
		return
	surface.set_mount_failure_injection(-1, -1)
	action("recover")
	await frames()
	if not expect_opacity(0.9, "recovery lost the rejected declarative change"):
		return
	action("direct")
	await frames()
	action("clear")
	await frames()
	if not expect_opacity(0.9, "clearing an override did not restore declarative props"):
		return
	action("direct")
	await frames()
	action("remove-leaf")
	await frames()
	if not expect(not is_instance_valid(host), "removed host remained mounted"):
		return
	action("restore-leaf")
	await frames()
	host = surface.get_node("ReactNativeMountContainer").get_child(0)
	original_id = host.get_instance_id()
	if not expect_opacity(0.9, "removed host retained a stale direct override"):
		return
	for hidden_node in [surface, self, host]:
		hidden_node.hide()
		action("clear-events")
		await click()
		if not expect(HermesRuntime.get_global("__godotMountingRegressionEvents").is_empty(), "hidden %s received input" % hidden_node.name):
			return
		hidden_node.show()
		await click()
		if not expect(HermesRuntime.get_global("__godotMountingRegressionEvents").has("topClick"), "shown %s did not receive input" % hidden_node.name):
			return
	print("RN_SMOKE_OK: mounting-regressions")
	get_tree().quit(0)
