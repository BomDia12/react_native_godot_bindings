extends Node

signal sampled(temperature: float, label: String)
signal referenced(node: Object)
var temperature := 18.5
var label := "sensor"
var malformed := false
var ui: ReactNativeRootView

func snapshot() -> Dictionary:
	return {"temperature": "invalid" if malformed else temperature, "label": label}

func adjust(amount: float = 0.5) -> float:
	temperature += amount
	sampled.emit(temperature, label)
	return temperature

func object_round_trip(value: Dictionary) -> Dictionary:
	assert(value.node == self and value.items[0] == self and value.items[1] == null)
	referenced.emit(self)
	return value

func restart_ui() -> void:
	ui.reload()

func binding() -> RNSceneBinding:
	var resource := RNSceneBinding.new()
	resource.capability = "ClimateSensor"
	resource.snapshot_method = &"snapshot"
	resource.snapshot_schema = {"type": "record", "fields": {"temperature": {"type": "number"}, "label": {"type": "string"}}}
	var object_schema := {"type": "Object", "capability": "ClimateSensor", "nullable": true}
	var object_record := {"type": "record", "fields": {"node": object_schema, "items": {"type": "array", "element": object_schema}}}
	resource.commands = {
		"adjust": {"method": "adjust", "mode": "sync", "arguments": [{"name": "amount", "value": {"type": "number"}, "optional": true}], "result": {"type": "number"}},
		"objects": {"method": "object_round_trip", "mode": "sync", "arguments": [{"name": "value", "value": object_record}], "result": object_record},
		"objectsAsync": {"method": "object_round_trip", "mode": "queued", "arguments": [{"name": "value", "value": object_record}], "result": object_record},
		"restart": {"method": "restart_ui", "mode": "sync", "arguments": [], "result": {"type": "void"}}}
	resource.signals = {"sampled": {"event": "sample", "arguments": ["temperature", "label"],
		"payload": resource.snapshot_schema}, "referenced": {"event": "reference", "arguments": ["node"], "payload": {"type": "record", "fields": {"node": object_schema}}}}
	return resource
