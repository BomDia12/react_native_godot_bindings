extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "native-module-interop"

func schema(type: String) -> Dictionary:
	return {"type": type}

func echo_binding() -> RNSceneBinding:
	var value := {"type": "record", "fields": {
		"color": schema("Color"),
		"position": schema("Vector2"),
		"integer": schema("int64"),
		"bytes": schema("Uint8Array"),
	}}
	var binding := RNSceneBinding.new()
	binding.capability = "ExampleCounter"
	binding.snapshot_method = &"read"
	binding.snapshot_schema = {"type": "record", "fields": {"value": schema("number"), "position": schema("Vector2")}}
	binding.commands = {"echo": {"method": "echo", "mode": "sync", "arguments": [{"name": "value", "value": value}], "result": value}}
	return binding

func find_counters() -> Array[RNExampleCounter]:
	var counters: Array[RNExampleCounter] = []
	for child in find_children("*", "RNExampleCounter", true, false):
		counters.push_back(child)
	return counters

func _ready() -> void:
	for root: ReactNativeRootView in [self, $PeerRoot]:
		var error := root.attach_scene_binding(root.get_node("Counter"), echo_binding())
		if not error.is_empty():
			fail_smoke("scene binding did not attach: %s" % [error])
			return
	super._ready()

func validate_state(state: Dictionary) -> String:
	if state.get("error") != null:
		return "native async call rejected: %s" % [state.error]
	if state.get("constants", {}).get("name") != "ExampleScene":
		return "TurboModule lookup did not reach ExampleScene"
	if state.get("before", {}).get("value") != 0.0:
		return "synchronous read did not reach the root's counter"
	var echo = state.get("echo", {})
	if not echo.get("color", Color()).is_equal_approx(Color(0.2, 0.4, 0.6, 1.0)):
		return "typed Color did not round trip"
	if echo.get("position") != Vector2(3, 4) or echo.get("integer") != 9223372036854775807:
		return "typed Vector2/int64 did not round trip: %s" % [echo]
	if echo.get("bytes") != PackedByteArray([2, 3]):
		return "Uint8Array slice did not round trip"
	if state.get("result", {}).get("value") != 2.0 or state.get("event", {}).get("value") != 2.0:
		return "Promise/event delivery did not complete"
	if state.get("removedResult", {}).get("value") != 3.0 or state.get("event", {}).get("value") != 2.0:
		return "removed subscription received a later event"
	return ""

func ready_to_validate():
	var states = await js("__godotNativeModuleStates")
	if not states is Dictionary or states.size() != 2:
		return false
	for state in states.values():
		if state.get("error") != null:
			return true
		if not state.get("removedResult") is Dictionary:
			return false
	return true

func validate_smoke():
	var states = await js("__godotNativeModuleStates")
	for state in states.values():
		var failure := validate_state(state)
		if not failure.is_empty():
			return failure
	var counters := find_counters()
	if counters.size() != 2:
		return "two registered native counters were not found"
	for counter in counters:
		if counter.read().get("value") != 3.0:
			return "registered native counter was not independently mutated"
	return ""
