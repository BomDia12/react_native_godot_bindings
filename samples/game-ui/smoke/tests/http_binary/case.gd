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
	if not await check_image_requests():
		return
	var records := FileAccess.get_file_as_string(network.records)
	check(records.contains('"path": "/icon.png"') and records.contains('"path": "/echo"'), "Server records lack real image/upload requests")

func http_active() -> int:
	return int(ReactNativeHost.get_service_stats().http.active)

func requested_loads() -> Array:
	return (await fixture()).events.filter(func(event): return String(event).begins_with("requested-load"))

# A slow request that its Image replaces or unmounts reports nothing and leaves
# the HTTP pool before the fixture answers.
func check_image_requests() -> bool:
	await action("requestImage", [network.http + "/slow?seconds=1"])
	if not await wait_until(func(): return http_active() > 0, "the slow image request"):
		return false
	await action("requestImage", [network.http + "/icon.png"])
	if not await wait_until(func(): return (await requested_loads()).size() == 1, "the replacing image"):
		return false
	await get_tree().create_timer(1.5).timeout
	var loads := await requested_loads()
	check(loads.size() == 1 and String(loads[0]).ends_with("/icon.png"), "A replaced image request reported onLoad: %s" % [loads])

	await action("requestImage", [network.http + "/slow?seconds=2"])
	var started := Time.get_ticks_msec()
	if not await wait_until(func(): return http_active() > 0, "the unmounted image's request"):
		return false
	await get_tree().create_timer(0.1).timeout
	await action("requestImage", [null])
	if not await wait_until(func(): return http_active() == 0, "the unmounted image's request to leave the pool"):
		return false
	check(Time.get_ticks_msec() - started < 1800, "Unmounting an Image did not cancel its HTTP request before the server answered")
	await get_tree().create_timer(2.2).timeout
	check((await requested_loads()).size() == 1, "An unmounted Image reported onLoad: %s" % [await requested_loads()])
	return true

