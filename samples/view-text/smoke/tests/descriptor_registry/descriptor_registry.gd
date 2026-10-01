extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "descriptor-registry"

func find_meter(node: Node) -> ProgressBar:
	if node is ProgressBar:
		return node
	for child in node.get_children():
		var result := find_meter(child)
		if result != null:
			return result
	return null

func validate_smoke() -> String:
	var state = HermesRuntime.get_global("__godotDescriptorState")
	if not state is Dictionary or not state.get("registered", false) or not state.get("unknownRejected", false):
		return "descriptor registry lookup failed"
	var meter := find_meter(self)
	if meter == null:
		return "RNExampleMeter did not create a ProgressBar"
	if meter.size.x != 80.0 or meter.size.y != 20.0:
		return "intrinsic meter size was %s" % [meter.size]
	if not is_equal_approx(meter.value, 0.5):
		return "advance command did not preserve native override: %s with state %s" % [meter.value, state]
	if not is_equal_approx(state.get("eventValue", -1.0), 0.5):
		return "native value_changed event did not reach React"
	var measure = state.get("measure")
	if not measure is Dictionary or measure.get("width") != 80.0 or measure.get("height") != 20.0:
		return "public measure did not report descriptor geometry"
	return ""
