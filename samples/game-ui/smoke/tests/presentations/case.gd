extends "res://smoke/game_test.gd"

var root_requests: Array = []
var app_requests: Array = []
var custom: ReactNativeRootView
var late_request := ""
func _ready() -> void:
	super._ready()
	panels[0].set_alert_handler(func(id: String, origin, payload: Dictionary):
		root_requests.append({"id": id, "origin": origin, "payload": payload})
		late_request = id
		check(panels[0].complete_alert(id, {"buttonId": 0, "dismissed": false}), "Root Godot handler could not complete its request"))
	GodotAlerts.set_alert_handler(func(id: String, origin, payload: Dictionary):
		app_requests.append({"id": id, "origin": origin, "payload": payload})
		check(GodotAlerts.complete_alert(id, {"buttonId": 0, "dismissed": false}), "Application handler could not complete request"))
func dialogs(node: Node) -> Array[Node]:
	var result: Array[Node] = []
	if node is AcceptDialog: result.append(node)
	for child in node.get_children(): result.append_array(dialogs(child))
	return result
func _process(_delta: float) -> void:
	if not advance(): return
	match stage:
		0:
			HermesRuntime.call_function("__gameAction", [panels[0].get_root_tag(), "testAlert", []])
			next_stage()
		1:
			check(root_requests.size() == 1 and "enemy-alert-button" in fixture().events, "Disabled enemy did not bubble to its Godot handler")
			check(dialogs(inventory_root).is_empty(), "Enabled sibling presented an enemy alert")
			check(not inventory_root.complete_alert(late_request, {"buttonId": 0, "dismissed": false}), "Another root completed a foreign request")
			action("testAlert")
			next_stage()
		2:
			var local := dialogs(inventory_root)
			check(local.size() == 1 and dialogs(panels[0]).is_empty(), "Enabled Alert was not local to the inventory root")
			if local.size() == 1: local[0].close_requested.emit()
			next_stage()
		3:
			var local := dialogs(inventory_root)
			check(local.size() == 1 and local[0].visible and fixture().events.count("inventory-alert-button") == 0, "Noncancelable alert was hidden or settled by window close")
			if local.size() == 1: local[0].confirmed.emit()
			next_stage()
		4:
			check(fixture().events.count("inventory-alert-button") == 1 and dialogs(inventory_root).is_empty(), "Local alert callback/lifetime did not settle once")
			action("ambientAlert")
			next_stage()
		5:
			check(app_requests.size() == 1 and app_requests[0].origin == null and "ambient-alert-button" in fixture().events, "Unattributed Alert selected an RN root")
			custom = ReactNativeRootView.new()
			custom.application_key = "CustomInventory"
			custom.size = Vector2(800, 600)
			assert(custom.attach_scene_binding(inventory, inventory.binding()).is_empty())
			ui.add_child(custom)
			next_stage()
		6:
			HermesRuntime.call_function("__gameAction", [custom.get_root_tag(), "testAlert", ["Custom"]])
			next_stage()
		7:
			check(fixture().events.count("custom-presenter") == 1 and dialogs(custom).is_empty(), "Custom presenter did not replace native presentation")
			HermesRuntime.call_function("__gameAction", [custom.get_root_tag(), "testAlert", ["Decline"]])
			next_stage()
		8:
			check(app_requests.size() == 2 and app_requests[1].origin.rootTag == custom.get_root_tag(), "Explicit custom decline did not bubble once")
			custom.queue_free()
			HermesRuntime.call_function("__gameAction", [panels[0].get_root_tag(), "saveAlert", []])
			panels[0].reload()
			next_stage()
		9:
			action("staleAlert")
			action("window", [true])
			next_stage()
		10:
			check(fixture().get("alertError") == "E_CANCELLED" and root_requests.size() == 1, "Stale origin was reassigned or not canceled")
			action("window", [false])
			GodotAlerts.set_alert_handler(Callable())
			finish("game-presentations")
