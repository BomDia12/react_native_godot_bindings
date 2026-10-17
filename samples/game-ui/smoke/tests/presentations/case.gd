extends "res://smoke/game_test.gd"

var root_requests: Array = []
var app_requests: Array = []
var late_request := ""

func smoke_id() -> String:
	return "game-presentations"

func dialogs(node: Node) -> Array[Node]:
	var result: Array[Node] = []
	if node is AcceptDialog:
		result.append(node)
	for child in node.get_children():
		result.append_array(dialogs(child))
	return result

func events() -> Array:
	return (await fixture()).get("events", [])

func _ready() -> void:
	super._ready()
	panels[0].set_alert_handler(func(id: String, origin, payload: Dictionary):
		root_requests.append({"id": id, "origin": origin, "payload": payload})
		late_request = id
		check(panels[0].complete_alert(id, {"buttonId": 0, "dismissed": false}), "Root Godot handler could not complete its request"))
	GodotAlerts.set_alert_handler(func(id: String, origin, payload: Dictionary):
		app_requests.append({"id": id, "origin": origin, "payload": payload})
		check(GodotAlerts.complete_alert(id, {"buttonId": 0, "dismissed": false}), "Application handler could not complete request"))

func run() -> void:
	await root_action(panels[0], "testAlert")
	if not await wait_until(func(): return root_requests.size() == 1 and "enemy-alert-button" in await events(), "the enemy alert to bubble to its Godot handler"):
		return
	check(dialogs(inventory_root).is_empty(), "Enabled sibling presented an enemy alert")
	check(not inventory_root.complete_alert(late_request, {"buttonId": 0, "dismissed": false}), "Another root completed a foreign request")

	await action("testAlert")
	if not await wait_until(func(): return dialogs(inventory_root).size() == 1, "the native inventory alert"):
		return
	check(dialogs(panels[0]).is_empty(), "Enabled Alert was not local to the inventory root")
	var local: AcceptDialog = dialogs(inventory_root)[0]
	local.close_requested.emit()
	await frames(4)
	check(dialogs(inventory_root).size() == 1 and local.visible and (await events()).count("inventory-alert-button") == 0, "Noncancelable alert was hidden or settled by window close")
	local.confirmed.emit()
	if not await wait_until(func(): return (await events()).count("inventory-alert-button") == 1 and dialogs(inventory_root).is_empty(), "the local alert to settle once"):
		return

	await action("ambientAlert")
	if not await wait_until(func(): return app_requests.size() == 1 and "ambient-alert-button" in await events(), "Alert.alert to reach the application handler"):
		return
	check(app_requests[0].origin == null, "Alert.alert selected a React Native root")

	var custom := ReactNativeRootView.new()
	custom.application_key = "CustomInventory"
	custom.size = Vector2(800, 600)
	assert(custom.attach_scene_binding(inventory, inventory.binding()).is_empty())
	ui.add_child(custom)
	if not await wait_until(func(): return custom.get_root_tag() != 0 and await js("__gameAction !== undefined") and target("inventory-editor", custom) != null, "the custom presenter root"):
		return
	await root_action(custom, "testAlert", ["Custom"])
	if not await wait_until(func(): return (await events()).count("custom-presenter") == 1, "the custom presenter"):
		return
	check(dialogs(custom).is_empty(), "Custom presenter did not replace native presentation")
	await root_action(custom, "testAlert", ["Decline"])
	if not await wait_until(func(): return app_requests.size() == 2, "the declined alert to bubble"):
		return
	check(app_requests[1].origin.rootTag == custom.get_root_tag(), "Explicit custom decline did not bubble once")
	custom.queue_free()

	await root_action(panels[0], "saveAlert")
	panels[0].reload()
	await frames(4)
	await action("staleAlert")
	await action("window", [true])
	if not await wait_until(func(): return (await fixture()).get("alertError") == "E_CANCELLED", "the stale alert to be canceled"):
		return
	check(root_requests.size() == 1, "Stale root alert was reassigned")
	await action("window", [false])
	GodotAlerts.set_alert_handler(Callable())
