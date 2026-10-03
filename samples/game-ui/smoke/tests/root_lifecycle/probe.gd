extends Node

signal sampled(temperature: float, label: String)
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

func restart_ui() -> void:
	ui.reload()

func binding() -> RNSceneBinding:
	var resource := RNSceneBinding.new()
	resource.capability = "ClimateSensor"
	resource.snapshot_method = &"snapshot"
	resource.snapshot_schema = {"type": "record", "fields": {"temperature": {"type": "number"}, "label": {"type": "string"}}}
	resource.commands = {
		"adjust": {"method": "adjust", "mode": "sync", "arguments": [{"name": "amount", "value": {"type": "number"}, "optional": true}], "result": {"type": "number"}},
		"restart": {"method": "restart_ui", "mode": "sync", "arguments": [], "result": {"type": "void"}}}
	resource.signals = {"sampled": {"event": "sample", "arguments": ["temperature", "label"],
		"payload": resource.snapshot_schema}}
	return resource
