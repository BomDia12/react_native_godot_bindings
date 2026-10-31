extends "res://smoke/communication_case.gd"

# A GDScript object registered as an application module answers every root,
# survives a hard reload, and is gone once unregistered. A module's node
# outside the tree becomes ready when it enters.

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
		return

	# A module whose node isn't in the tree yet becomes ready when it enters.
	var scoreboard := GDScript.new()
	scoreboard.source_code = "extends Node\nsignal score_changed(score: int)\nfunc snapshot() -> Dictionary:\n\treturn {\"score\": 0}\nfunc add(points: int) -> int:\n\treturn points\n"
	scoreboard.reload()
	var later := Node.new()
	later.set_script(scoreboard)
	if not ReactNativeHost.register_module("Later", later, binding()).is_empty():
		fail("registering a module for a node outside the tree failed")
		return
	if await js("__moduleReady('Later')"):
		fail("a module whose node is outside the tree was ready")
		return
	add_child(later)
	if not await wait_until(func(): return await js("__moduleReady('Later')"), "the module once its node entered the tree"):
		return
	ReactNativeHost.unregister_module("Later")
	later.queue_free()
