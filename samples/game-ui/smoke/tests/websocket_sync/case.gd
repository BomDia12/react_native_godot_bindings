extends "res://smoke/game_test.gd"

var network: Dictionary

func smoke_id() -> String:
	return "game-websocket-sync"

func _ready() -> void:
	network = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("GODOT_SMOKE_NETWORK_FIXTURE")))
	super._ready()

func run() -> void:
	await action("socketChecks", [network.websocket])
	if not await wait_until(func():
		var state := await fixture()
		return state.has("socketEvidence") or state.has("networkError"), "the WebSocket checks"):
		return
	var state := await fixture()
	if state.has("networkError"):
		check(false, state.networkError)
		return
	check(state.socketEvidence.size() == 9, "Native WebSocket evidence is incomplete: %s" % [state.socketEvidence])
	check(inventory.items[0].quantity == 7 and state.inventory.items[0].quantity == 7, "Socket update bypassed Godot authority")
	var records := FileAccess.get_file_as_string(network.records)
	check(records.contains('"websocket": "received"') and records.contains('"binary": true'), "Server did not observe binary frames")
	if not await wait_until(func(): return int(ReactNativeHost.get_service_stats().websocket_peers) == 0, "closed sockets to release their peers"):
		return
	await js("globalThis.resetSocket=new WebSocket(%s);globalThis.resetSocketOpened=false;resetSocket.onopen=()=>{resetSocketOpened=true;};undefined;" % JSON.stringify(network.websocket))
	if not await wait_until(func(): return await js("resetSocketOpened") == true, "the reset socket to open"):
		return
	var generation := ReactNativeHost.get_generation()
	ReactNativeFileSingleton.force_refresh()
	if not await wait_until(func(): return ReactNativeHost.get_generation() == generation + 1 and await game_ready(), "the bundle reset"):
		return
	check(int(ReactNativeHost.get_service_stats().websocket_peers) == 0, "Reset retained an old native peer")
	check(inventory.items[0].quantity == 7 and (await fixture()).inventory.items[0].quantity == 7, "Reset lost authoritative network state")
