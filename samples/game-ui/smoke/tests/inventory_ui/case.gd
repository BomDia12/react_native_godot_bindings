extends "res://smoke/game_test.gd"

func smoke_id() -> String:
	return "game-inventory-ui"

func run() -> void:
	if not await wait_until(func(): return target("asset-image") is RNImageControl and target("asset-image").texture != null, "the inventory asset"):
		return
	check(target("section-list") is ScrollContainer, "SectionList did not mount")
	var editor_id := target("inventory-editor").get_child(0).get_instance_id()
	var list := target("inventory-list") as ScrollContainer
	list.scroll_vertical = 120
	var offset := list.scroll_vertical
	await action("command", ["equip", ["potion"]])
	await action("controlled", ["Reconciled"])
	if not await wait_until(func(): return inventory.items[0].equipped and (await fixture()).inventory.items[0].equipped and target("controlled-editor").get_child(0).text == "Reconciled", "the equip command and controlled edit"):
		return
	check(target("inventory-editor").get_child(0).get_instance_id() == editor_id, "Inventory update remounted editor")
	check((target("inventory-list") as ScrollContainer).scroll_vertical == offset, "Update lost scroll offset")
	await action("scroll", [50])
	if not await wait_until(func(): return (target("inventory-list") as ScrollContainer).scroll_vertical > 1000 and "item-50" in (await fixture()).rendered, "the FlatList virtual window"):
		return
	await action("command", ["rename", ["Godot inventory"]])
	await action("modal", [true])
	if not await wait_until(func(): return inventory.title == "Godot inventory" and "modal-show" in (await fixture()).events, "the rename and modal"):
		return
	await action("modal", [false])
