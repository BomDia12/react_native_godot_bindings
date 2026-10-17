extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "component-registry"

func find_meter(node: Node) -> ProgressBar:
	if node is ProgressBar:
		return node
	for child in node.get_children():
		var result := find_meter(child)
		if result != null:
			return result
	return null

func ready_to_validate():
	var state = await js("__godotComponentRegistryState")
	return state is Dictionary and state.get("measure") is Dictionary and state.get("eventValue") != -1

func validate_smoke():
	var state = await js("__godotComponentRegistryState")
	if not state.get("registered", false) or not state.get("unknownRejected", false):
		return "component registry lookup failed"
	var meter := find_meter(mount_container())
	if meter == null:
		return "RNExampleMeter did not create a ProgressBar"
	if meter.size != Vector2(80, 20):
		return "explicit meter size was %s" % [meter.size]
	if not is_equal_approx(meter.value, 0.5):
		return "advance command did not change the native value: %s with state %s" % [meter.value, state]
	if not is_equal_approx(state.get("eventValue", -1.0), 0.5):
		return "native value_changed event did not reach React"
	var measure = state.get("measure")
	if measure.get("width") != 80.0 or measure.get("height") != 20.0:
		return "public measure did not report the meter geometry"
	return ""
