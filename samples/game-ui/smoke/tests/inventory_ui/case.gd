extends "res://smoke/game_test.gd"

var editor_id := 0
var offset := 0
func _process(_delta: float) -> void:
	if not advance(): return
	match stage:
		0:
			check(target("asset-image") is RNImageControl and target("asset-image").texture != null, "Inventory asset did not load")
			check(target("section-list") is ScrollContainer, "SectionList did not mount")
			editor_id = target("inventory-editor").get_child(0).get_instance_id()
			var list := target("inventory-list") as ScrollContainer
			list.scroll_vertical = 120
			offset = list.scroll_vertical
			action("command", ["equip", ["potion"]])
			action("controlled", ["Reconciled"])
			next_stage()
		1:
			check(inventory.items[0].equipped and fixture().inventory.items[0].equipped, "Equip command did not reconcile")
			check(target("inventory-editor").get_child(0).get_instance_id() == editor_id, "Inventory update remounted editor")
			check((target("inventory-list") as ScrollContainer).scroll_vertical == offset, "Update lost scroll offset")
			check(target("controlled-editor").get_child(0).text == "Reconciled", "Controlled editing did not reconcile")
			action("scroll", [50])
			next_stage()
		2:
			check((target("inventory-list") as ScrollContainer).scroll_vertical > 1000 and "item-50" in fixture().rendered, "FlatList did not move its virtual window")
			action("command", ["rename", ["Godot inventory"]])
			action("modal", [true])
			next_stage()
		3:
			check(inventory.title == "Godot inventory", "Rename command did not reach Godot")
			check("modal-show" in fixture().events, "Modal callback did not run")
			action("modal", [false])
			finish("game-inventory-ui")
