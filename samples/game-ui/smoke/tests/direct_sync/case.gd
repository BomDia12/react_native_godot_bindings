extends "res://smoke/game_test.gd"

func _process(_delta: float) -> void:
	if not advance(): return
	match stage:
		0:
			enemies[1].apply_damage(17)
			inventory.set_quantity("potion", 8)
			next_stage()
		1:
			var values: Dictionary = fixture().enemies
			for index in range(3):
				check(values[str(panels[index].get_root_tag())].health == (83 if index == 1 else 100), "Damage changed the wrong enemy surface")
			check(fixture().inventory.items[0].quantity == 8, "Godot pickup did not reach React")
			action("command", ["use", ["potion"]])
			next_stage()
		2:
			check(inventory.items[0].quantity == 7 and fixture().inventory.items[0].quantity == 7, "React command did not mutate Godot inventory")
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotHTTP').stats().started") == 0, "Offline synchronization leased a network request")
			check(fixture().network.is_empty(), "Direct synchronization made a network call")
			finish("game-direct-sync")
