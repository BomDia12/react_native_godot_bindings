extends "res://smoke/communication_case.gd"

# Untyped messages travel in order between GDScript and one root, and between
# GDScript and the application.

var pongs: Array = []
var acks: Array = []

func smoke_id() -> String:
	return "game-messages"

func run() -> void:
	var root := add_root("GodotMessagesApp")
	root.message_received.connect(func(name, payload): if name == "pong": pongs.append(payload))
	ReactNativeHost.message_received.connect(func(name, payload): if name == "broadcast-ack": acks.append(payload))
	if not await wait_until(func(): return await js("__communication.messagesReady === true"), "the message listener"):
		return
	for i in 3:
		root.post_message("ping", {"index": i, "color": Color(0, 1, 0)})
	ReactNativeHost.post_message("broadcast", [1, "two"])
	if not await wait_until(func(): return pongs.size() == 3 and acks.size() == 1, "replies to the messages"):
		return
	if pongs.map(func(payload): return payload["index"]) != [0, 1, 2] or pongs[0]["color"] != Color(0, 1, 0):
		fail("root messages arrived as %s" % [pongs])
		return
	if acks[0] != [1, "two"]:
		fail("the application message arrived as %s" % [acks[0]])
