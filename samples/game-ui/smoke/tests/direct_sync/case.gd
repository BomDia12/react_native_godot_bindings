extends "res://smoke/game_test.gd"

var death_count := 0

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
			enemies[0].died.connect(func(): death_count += 1)
			enemies[0].apply_damage(100)
			next_stage()
		3:
			check(death_count == 1 and not is_instance_valid(panels[0]), "Enemy death did not remove its panel once")
			enemies[0].apply_damage(10)
			enemies[0].apply_damage(0)
			enemies[0].apply_damage(-10)
			check(death_count == 1, "Repeated damage emitted another death at zero health")
			enemies[0].heal(10)
			enemies[0].apply_damage(10)
			check(death_count == 2, "A new positive-to-zero transition did not emit death")
			check(is_instance_valid(panels[1]) and is_instance_valid(inventory_root), "Dead enemy damage changed a sibling surface")
			finish("game-direct-sync")
