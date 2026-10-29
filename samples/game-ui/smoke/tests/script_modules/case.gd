extends "res://smoke/communication_case.gd"

# The Scoreboard module of the autolinked react-native-godot-script-example
# library answers every root, survives a hard reload, and is gone once
# unregistered. A module's node outside the tree becomes ready when it enters.

func smoke_id() -> String:
	return "game-script-modules"

func last_score() -> Variant:
	var scores = await js("__communication.scores")
	return scores.back() if scores is Array and not scores.is_empty() else null

func run() -> void:
	var first := add_root("GodotModuleApp")
	var second := add_root("GodotModuleApp")
	if not await wait_until(func(): return mounted(first) and mounted(second), "the module roots"):
		return
	var both_read := "Boolean(__communication.scoreboards[%d] && __communication.scoreboards[%d])" % [first.get_root_tag(), second.get_root_tag()]
	if not await wait_until(func(): return await js(both_read) and await last_score() == 0, "useScoreboard to read the module in both roots"):
		return
	if await js("__scoreboardAdd(%d, 5)" % first.get_root_tag()) != 5:
		fail("a synchronous module command did not return its result")
		return
	await js("__scoreboardAddLater(%d, 2)" % second.get_root_tag())
	if not await wait_until(func(): return await js("__communication.asyncScores[0]") == 7, "an asynchronous module command from the second root"):
		return
	if not await wait_until(func(): return await last_score() == 7, "the module's change event"):
		return

	ReactNativeHost.reload()
	if not await wait_until(func(): return mounted(first) and await js("typeof __moduleReady === 'function' && __moduleReady('Scoreboard')"), "the module after a reload"):
		return
	if not await wait_until(func(): return await last_score() == 7, "the module's state after a reload"):
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
	if not ReactNativeHost.register_module("Later", later, load("res://addons/godot_script_example/scoreboard_binding.tres")).is_empty():
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
