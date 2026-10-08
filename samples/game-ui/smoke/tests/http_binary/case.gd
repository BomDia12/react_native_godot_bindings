extends "res://smoke/game_test.gd"

var network: Dictionary
var previous_max_fps := 0
func _ready() -> void:
	super._ready()
	previous_max_fps = Engine.max_fps
	network = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("GODOT_SMOKE_NETWORK_FIXTURE")))
func _process(_delta: float) -> void:
	if not advance(): return
	if fixture().has("networkError"):
		check(false, fixture().networkError)
		Engine.max_fps = previous_max_fps
		finish("game-http-binary")
		return
	match stage:
		0:
			Engine.max_fps = 5
			action("lowFPSDownload", [network.http])
			next_stage()
		1:
			if not fixture().has("lowFPSDownload"): return
			Engine.max_fps = previous_max_fps
			action("httpChecks", [network.http, network.https])
			next_stage()
		2:
			if not fixture().has("httpEvidence"): return
			check(fixture().httpEvidence.size() >= 15, "HTTP/binary contract evidence is incomplete")
			action("refresh", [network.http])
			next_stage()
		3:
			if not "remote-load" in fixture().events: return
			check(inventory.items[0].quantity == 4 and fixture().inventory.items[0].quantity == 4, "HTTP inventory bypassed Godot authority")
			check(target("remote-image") is RNImageControl and target("remote-image").texture != null, "Shared HTTP image transport did not decode")
			var records := FileAccess.get_file_as_string(network.records)
			check(records.contains('"path": "/icon.png"') and records.contains('"path": "/echo"'), "Server records lack real image/upload requests")
			finish("game-http-binary")
