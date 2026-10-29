extends "res://smoke/communication_case.gd"

# A GDScript object registered as an application module answers every root,
# survives a hard reload, and is gone once unregistered.

signal score_changed(score: int)

var score := 0

func smoke_id() -> String:
	return "game-script-modules"

func snapshot() -> Dictionary:
	return {"score": score}

func add(points: int) -> int:
	score += points
	score_changed.emit(score)
	return score

func binding() -> RNSceneBinding:
	var resource := RNSceneBinding.new()
	resource.capability = "Scoreboard"
	resource.snapshot_method = &"snapshot"
	resource.snapshot_schema = {"type": "record", "fields": {"score": {"type": "integer"}}}
	resource.commands = {"add": {"method": "add", "mode": "sync", "arguments": [{"name": "points", "value": {"type": "integer"}}], "result": {"type": "integer"}}}
	resource.signals = {"score_changed": {"event": "changed", "arguments": ["score"], "payload": {"type": "record", "fields": {"score": {"type": "integer"}}}}}
	return resource

func last_snapshot() -> Variant:
	var snapshots = await js("__communication.moduleSnapshots")
	return snapshots.back() if snapshots is Array and not snapshots.is_empty() else null

func run() -> void:
	var root := add_root("GodotModuleApp")
	if not await wait_until(func(): return mounted(root), "the module root"):
		return
	if await js("__moduleReady('Scoreboard')"):
		fail("an unregistered module was ready")
		return
	var error := ReactNativeHost.register_module("Scoreboard", self, binding())
	if not error.is_empty():
		fail("registering the module failed: %s" % error)
		return
	if not await wait_until(func(): return await last_snapshot() == {"score": 0}, "useGodotModule to read the module"):
		return
	if await js("__moduleCall('Scoreboard', 'add', [5])") != 5:
		fail("a synchronous module command did not return its result")
		return
	if not await wait_until(func(): return await last_snapshot() == {"score": 5}, "the module's change event"):
		return

	ReactNativeHost.reload()
	if not await wait_until(func(): return mounted(root) and await js("typeof __moduleReady === 'function' && __moduleReady('Scoreboard')"), "the module after a reload"):
		return
	ReactNativeHost.unregister_module("Scoreboard")
	if await js("__moduleReady('Scoreboard')"):
		fail("an unregistered module stayed ready")
