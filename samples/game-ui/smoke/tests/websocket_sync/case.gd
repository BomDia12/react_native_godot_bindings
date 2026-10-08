extends "res://smoke/game_test.gd"

var network: Dictionary
func _ready() -> void:
	super._ready()
	network = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("GODOT_SMOKE_NETWORK_FIXTURE")))
func _process(_delta: float) -> void:
	if not advance(): return
	if fixture().has("networkError"):
		check(false, fixture().networkError)
		finish("game-websocket-sync")
		return
	match stage:
		0:
			action("socketChecks", [network.websocket])
			next_stage()
		1:
			if not fixture().has("socketEvidence"): return
			check(fixture().socketEvidence.size() == 10, "Native WebSocket evidence is incomplete")
			check(inventory.items[0].quantity == 7 and fixture().inventory.items[0].quantity == 7, "Socket update bypassed Godot authority")
			var records := FileAccess.get_file_as_string(network.records)
			check(records.contains('"websocket": "received"') and records.contains('"binary": true'), "Server did not observe binary frames")
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotWebSocket').stats().peers") == 0, "Closed sockets retained peers")
			HermesRuntime.evaluate("globalThis.resetSocket=new WebSocket(%s);globalThis.resetSocketOpened=false;resetSocket.onopen=()=>{resetSocketOpened=true;};undefined;" % JSON.stringify(network.websocket))
			next_stage()
		2:
			if not HermesRuntime.get_global("resetSocketOpened"): return
			ReactNativeFileSingleton.force_refresh()
			next_stage()
		3:
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotWebSocket').stats().peers") == 0, "Reset retained an old native peer")
			check(inventory.items[0].quantity == 7 and fixture().inventory.items[0].quantity == 7, "Reset lost authoritative network state")
			finish("game-websocket-sync")
