extends "res://smoke/support/smoke_case.gd"

const WAIT_SECONDS := 30

var metro: Dictionary

func smoke_id() -> String:
	return "dev-server"

# The root starts the runtime when it enters the tree, so the developer server
# settings have to be in place before that.
func _init() -> void:
	# Frame-based timeouts then cover the Metro download and the Fast Refresh round trip.
	Engine.max_fps = 60
	metro = JSON.parse_string(FileAccess.get_file_as_string(OS.get_environment("GODOT_SMOKE_NETWORK_FIXTURE")))
	ProjectSettings.set_setting("react_native/dev_server/enabled", true)
	ProjectSettings.set_setting("react_native/dev_server/host", metro.host)
	ProjectSettings.set_setting("react_native/dev_server/port", int(metro.port))
	ProjectSettings.set_setting("react_native/dev_server/entry", metro.entry)

func find_text(node: Node, prefix: String) -> RNTextControl:
	if node is RNTextControl and node.get_text().begins_with(prefix):
		return node
	for child in node.get_children():
		var found := find_text(child, prefix)
		if found != null:
			return found
	return null

func wait_for(condition: Callable) -> bool:
	for i in WAIT_SECONDS * 60:
		if condition.call():
			return true
		await get_tree().process_frame
	return condition.call()

func shows(text: String) -> bool:
	return get_root_tag() != 0 and find_text(mount_container(), text) != null

func log_box_root() -> ReactNativeRootView:
	for child in get_tree().root.get_children(true):
		if child is CanvasLayer and child.get_child_count() == 1 and child.get_child(0) is ReactNativeRootView:
			return child.get_child(0)
	return null

func fetch_json(url: String) -> Variant:
	var request := HTTPRequest.new()
	add_child(request)
	request.request(url)
	var response: Array = await request.request_completed
	request.queue_free()
	return JSON.parse_string(response[3].get_string_from_utf8()) if response[1] == 200 else null

func validate_smoke():
	if not shows("Fast Refresh before 0"):
		return "the Metro bundle did not render the app"
	await js("__godotDevServerBump()")
	if not await wait_for(shows.bind("Fast Refresh before 1")):
		return "the app did not rerender after a state change"
	var tag := get_root_tag()
	var generation := ReactNativeHost.get_generation()
	var path: String = metro.app.path_join("DevServerApp.js")
	var source := FileAccess.get_file_as_string(path)
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(source.replace("Fast Refresh before", "Fast Refresh after"))
	file.close()
	if not await wait_for(shows.bind("Fast Refresh after 1")):
		return "Fast Refresh did not apply the edit with the component state kept"
	if get_root_tag() != tag or ReactNativeHost.get_generation() != generation:
		return "Fast Refresh restarted the surface"
	var failure = await validate_log_box()
	if not String(failure).is_empty():
		return failure
	failure = await validate_inspector_reload()
	if not String(failure).is_empty():
		return failure
	return await validate_metro_reload()

# Debug builds report Platform.isTesting, which turns LogBox notifications
# off, so an uncaught error opens the inspector.
func validate_log_box():
	await js("setTimeout(() => { throw new Error('Godot LogBox smoke'); }, 0)")
	if not await wait_for(func(): return log_box_root() != null and log_box_root().get_root_tag() != 0 and log_box_root().get_child(0, true).get_child_count() > 0):
		return "an uncaught error did not open the LogBox inspector"
	await js("__godotLogBox.clearAllLogs()")
	if not await wait_for(func(): return log_box_root() == null):
		return "clearing the logs did not close the LogBox inspector"
	return ""

func validate_inspector_reload():
	var pages = null
	for attempt in WAIT_SECONDS:
		pages = await fetch_json(metro.metro + "/json/list")
		if pages is Array and not pages.is_empty():
			break
		await get_tree().create_timer(1.0).timeout
	if not pages is Array or pages.is_empty():
		return "Metro listed no inspector page for the runtime"
	var socket := WebSocketPeer.new()
	# Metro only accepts DevTools connections from its own origin, as a browser sends it.
	socket.handshake_headers = PackedStringArray(["Origin: " + metro.metro])
	socket.connect_to_url(pages[0].webSocketDebuggerUrl)
	if not await wait_for(func():
		socket.poll()
		return socket.get_ready_state() == WebSocketPeer.STATE_OPEN):
		return "DevTools could not connect to the inspector page"
	var tag := get_root_tag()
	var generation := ReactNativeHost.get_generation()
	socket.send_text(JSON.stringify({"id": 1, "method": "Page.reload"}))
	var replies: Array = []
	if not await wait_for(func():
		socket.poll()
		while socket.get_available_packet_count() > 0:
			replies.append(JSON.parse_string(socket.get_packet().get_string_from_utf8()))
		return replies.any(func(reply): return reply is Dictionary and reply.get("id") == 1.0 and reply.has("result"))):
		return "DevTools got no reply to Page.reload: %s" % [replies]
	if not await wait_for(func(): return ReactNativeHost.get_generation() == generation + 1 and get_root_tag() != tag and shows("Fast Refresh after 0")):
		return "Page.reload did not reload the runtime through ReactNativeHost"
	socket.close()
	if log_box_root() != null:
		return "the retired runtime's LogBox stayed open"
	return ""

# Metro's reload key broadcasts on /message, where the runtime's packager
# connection listens.
func validate_metro_reload():
	var socket := WebSocketPeer.new()
	socket.connect_to_url("ws://%s:%d/message" % [metro.host, int(metro.port)])
	if not await wait_for(func():
		socket.poll()
		return socket.get_ready_state() == WebSocketPeer.STATE_OPEN):
		return "could not connect to Metro's message socket"
	var tag := get_root_tag()
	var generation := ReactNativeHost.get_generation()
	socket.send_text(JSON.stringify({"version": 2, "method": "reload"}))
	if not await wait_for(func(): return ReactNativeHost.get_generation() == generation + 1 and get_root_tag() != tag and shows("Fast Refresh after 0")):
		return "Metro's reload message did not reload the runtime through ReactNativeHost"
	socket.close()
	if not ReactNativeHost.get_last_error().is_empty():
		return "the runtime reported an error: %s" % ReactNativeHost.get_last_error()
	return ""
