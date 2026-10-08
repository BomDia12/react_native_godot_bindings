extends RefCounted

static func value(type: String) -> Dictionary:
	return {"type": type}

static func record(fields: Dictionary) -> Dictionary:
	return {"type": "record", "fields": fields}

static func array(element: Dictionary) -> Dictionary:
	return {"type": "array", "element": element}

static func command(method: String, arguments: Array, result: Dictionary, mode: String = "sync") -> Dictionary:
	return {"method": method, "mode": mode, "arguments": arguments, "result": result}

static func argument(name: String, schema: Dictionary) -> Dictionary:
	return {"name": name, "value": schema}

static func binding(capability: String, snapshot_schema: Dictionary, commands: Dictionary) -> RNSceneBinding:
	var resource := RNSceneBinding.new()
	resource.capability = capability
	resource.snapshot_method = &"snapshot"
	resource.snapshot_schema = snapshot_schema
	resource.commands = commands
	resource.signals = {"changed": {"event": "changed", "arguments": ["state"],
		"payload": record({"state": snapshot_schema})}}
	return resource
