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
	if not await wait_until(func(): return http_active() > 0, "the unmounted image's request"):
		return false
	await get_tree().create_timer(0.1).timeout
	var unmounted := Time.get_ticks_msec()
	await action("requestImage", [null])
	if not await wait_until(func(): return http_active() == 0, "the unmounted image's request to leave the pool"):
		return false
	check(Time.get_ticks_msec() - unmounted < 500, "Cancelling an image's HTTP request stalled the game for %d ms" % (Time.get_ticks_msec() - unmounted))
	# The draining node keeps its response bytes in the buffer budget, and as
	# many one-second requests as the rest allows run at once beside it.
	var held := int(ReactNativeHost.get_service_stats().http.bufferedBytes)
	check(held > 0, "A draining request released its response budget early")
	var capacity := mini(int(ProjectSettings.get_setting("react_native/network/http/max_active_requests")), (int(ProjectSettings.get_setting("react_native/network/http/max_buffered_bytes")) - held) / int(ProjectSettings.get_setting("react_native/network/http/max_body_bytes")))
	var parallel_started := Time.get_ticks_msec()
	await action("parallelFetch", [network.http, capacity])
	if not await wait_until(func(): return (await fixture()).has("parallelFetch") or (await fixture()).has("networkError"), "requests started while a cancelled one drains"):
		return false
	check(Time.get_ticks_msec() - parallel_started < 1700, "Requests waited on a cancelled one: %d ms for %d one-second requests" % [Time.get_ticks_msec() - parallel_started, capacity])
	check(int(ReactNativeHost.get_service_stats().http.draining) == 1, "The cancelled request is not draining: %s" % [ReactNativeHost.get_service_stats().http])
	await get_tree().create_timer(1.5).timeout
	check((await requested_loads()).size() == 1, "An unmounted Image reported onLoad: %s" % [await requested_loads()])
	var drained: Dictionary = ReactNativeHost.get_service_stats().http
	check(int(drained.draining) == 0 and int(drained.bufferedBytes) == 0, "A cancelled request's node outlived its response: %s" % [drained])

	# A request that times out drains the same way instead of stalling the game.
	var longest := 0
	var previous := Time.get_ticks_msec()
	var timeout_started := previous
	await action("timeoutRequest", [network.http])
	while not (await fixture()).has("timedOut") and Time.get_ticks_msec() - timeout_started < 5000:
		await get_tree().process_frame
		longest = maxi(longest, Time.get_ticks_msec() - previous)
		previous = Time.get_ticks_msec()
	check((await fixture()).has("timedOut"), "An XHR timeout did not fire")
	check(longest < 500, "An HTTP timeout stalled the game for %d ms" % longest)
	return true

