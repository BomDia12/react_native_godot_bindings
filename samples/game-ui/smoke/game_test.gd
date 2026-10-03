extends "res://game/game.gd"

var frames := 0
var stage := 0
var stage_frame := 0
var failures: Array[String] = []

func fixture() -> Dictionary:
	var value = HermesRuntime.get_global("__game")
	return value if value is Dictionary else {}

func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)

func action(name: String, arguments: Array = []) -> void:
	HermesRuntime.call_function("__gameAction", [inventory_root.get_root_tag(), name, arguments])

func target(id: String, node: Node = null) -> Control:
	if node == null:
		node = self
	if node is Control and node.get_meta("react_native_test_id", "") == id:
		return node
	for child in node.get_children():
		var result := target(id, child)
		if result != null:
			return result
	return null

func ready() -> bool:
	return fixture().get("enemies", {}).size() == 3 and fixture().get("inventory") is Dictionary and target("inventory-editor") != null

func next_stage() -> void:
	stage += 1
	stage_frame = frames

func finish(id: String) -> void:
	for failure in failures:
		push_error(failure)
	if failures.is_empty():
		print("RN_SMOKE_OK: ", id)
	get_tree().quit(0 if failures.is_empty() else 1)

func advance() -> bool:
	frames += 1
	if frames > 1800:
		push_error("Timed out at stage %d; events %s" % [stage, fixture().get("events", [])])
		get_tree().quit(1)
		return false
	return frames - stage_frame >= 6 and (stage != 0 or ready())
