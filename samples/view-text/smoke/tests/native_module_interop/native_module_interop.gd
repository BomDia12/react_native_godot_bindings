extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "native-module-interop"

func find_counters() -> Array[RNExampleCounter]:
	var counters: Array[RNExampleCounter] = []
	for child in find_children("*", "RNExampleCounter", true, false):
		counters.push_back(child)
	return counters

func validate_state(state: Dictionary) -> String:
	if not state is Dictionary:
		return "native module fixture state is missing"
	if state.get("error") != null:
		return "native async call rejected: %s" % [state.error]
	if state.get("constants", {}).get("name") != "ExampleScene":
		return "TurboModule lookup did not reach ExampleScene"
	var schema = state.get("schema", {})
	if schema.get("methods", {}).get("incrementLater", {}).get("mode") != "async":
		return "native method schema metadata is missing"
	if schema.get("events", {}).get("onChanged", {}).get("name") != "changed":
		return "native event subscription metadata is missing"
	var echo = state.get("echo", {})
	if not echo.get("color", Color()).is_equal_approx(Color(0.2, 0.4, 0.6, 1.0)):
		return "typed Color did not round trip"
	if echo.get("position") != Vector2(3, 4) or echo.get("integer") != 9223372036854775807:
		return "typed Vector2/int64 did not round trip"
	if echo.get("bytes") != PackedByteArray([2, 3]):
		return "Uint8Array slice did not round trip"
	if state.get("result", {}).get("value") != 2.0 or state.get("event", {}).get("value") != 2.0:
		return "Promise/signal delivery did not complete"
	if state.get("cancelledCode") != "E_CANCELLED":
		return "AbortSignal cancellation did not reject with E_CANCELLED"
	if state.get("removedResult", {}).get("value") != 3.0 or state.get("event", {}).get("value") != 2.0:
		return "removed subscription received a later native signal"
	return ""

func validate_smoke() -> String:
	var states = HermesRuntime.get_global("__godotNativeModuleStates")
	if not states is Dictionary or states.size() != 2:
		return "two native-module roots did not publish isolated state"
	var sessions := {}
	var targets := {}
	for state in states.values():
		var failure := validate_state(state)
		if not failure.is_empty():
			return failure
		sessions[state.session] = true
		targets[state.target] = true
	if sessions.size() != 2 or targets.size() != 2:
		return "native-module roots reused a session or object handle"
	var counters := find_counters()
	if counters.size() != 2:
		return "two registered native counters were not found"
	for counter in counters:
		if counter.read().get("value") != 3.0:
			return "registered native counter was not independently mutated"
	return ""
