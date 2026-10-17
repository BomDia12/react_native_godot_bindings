extends "res://smoke/game_test.gd"

var network: Dictionary

func smoke_id() -> String:
	return "game-http-binary"

func _ready() -> void:
	network = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("GODOT_SMOKE_NETWORK_FIXTURE")))
	super._ready()

func network_step(key: String, label: String) -> bool:
	if not await wait_until(func():
		var state := await fixture()
		return state.has(key) or state.has("networkError"), label):
		return false
	var state := await fixture()
	if state.has("networkError"):
		check(false, state.networkError)
		return false
	return true

func run() -> void:
	var previous_max_fps := Engine.max_fps
	Engine.max_fps = 5
	await action("lowFPSDownload", [network.http])
	var downloaded := await network_step("lowFPSDownload", "the low-FPS download")
	Engine.max_fps = previous_max_fps
	if not downloaded:
		return
	await action("httpChecks", [network.http, network.https])
	if not await network_step("httpEvidence", "the HTTP checks"):
		return
	check((await fixture()).httpEvidence.size() >= 15, "HTTP/binary contract evidence is incomplete: %s" % [(await fixture()).httpEvidence])
	var stats := ReactNativeHost.get_service_stats()
	check(int(stats.blob_bytes) < 1024, "Native Blob storage was not reclaimed: %s" % [stats])
	check(int(stats.http.active) + int(stats.http.idle) + int(stats.http.draining) <= 8 and int(stats.http.active) <= 8, "HTTP pool exceeded its node bound: %s" % [stats.http])
	await action("refresh", [network.http])
	if not await wait_until(func(): return "remote-load" in (await fixture()).events, "the remote image"):
		return
	check(inventory.items[0].quantity == 4 and (await fixture()).inventory.items[0].quantity == 4, "HTTP inventory bypassed Godot authority")
	check(target("remote-image") is RNImageControl and target("remote-image").texture != null, "Shared HTTP image transport did not decode")
	var records := FileAccess.get_file_as_string(network.records)
	check(records.contains('"path": "/icon.png"') and records.contains('"path": "/echo"'), "Server records lack real image/upload requests")
