extends "res://smoke/game_test.gd"

var death_count := 0

func smoke_id() -> String:
	return "game-direct-sync"

func enemy_health(index: int) -> int:
	var values: Dictionary = (await fixture()).enemies
	return int(values.get(str(panels[index].get_root_tag()), {}).get("health", -1))

func run() -> void:
	await js("globalThis.immediateEvidence=[];const canceled=setImmediate(()=>immediateEvidence.push('canceled'));clearImmediate(canceled);setImmediate((text,value)=>{immediateEvidence.push([text,value]);setImmediate(()=>immediateEvidence.push(['nested']));},'ready',17);undefined;")
	check(ReactNativeHost.get_last_error().is_empty(), "Public RN immediate globals are unavailable")
	enemies[1].apply_damage(17)
	inventory.set_quantity("potion", 8)
	if not await wait_until(func(): return await enemy_health(1) == 83 and (await fixture()).inventory.items[0].quantity == 8, "Godot changes to reach React"):
		return
	check(await js("immediateEvidence.length===2 && immediateEvidence[0][0]==='ready' && immediateEvidence[0][1]===17 && immediateEvidence[1][0]==='nested'"), "Immediates lost cancellation, arguments or nested callbacks")
	check(await enemy_health(0) == 100 and await enemy_health(2) == 100, "Damage changed the wrong enemy surface")
	await action("command", ["use", ["potion"]])
	if not await wait_until(func(): return inventory.items[0].quantity == 7 and (await fixture()).inventory.items[0].quantity == 7, "the React command to mutate Godot inventory"):
		return
	check(int(ReactNativeHost.get_service_stats().http.started) == 0, "Offline synchronization leased a network request")
	check((await fixture()).network.is_empty(), "Direct synchronization made a network call")
	enemies[0].died.connect(func(): death_count += 1)
	enemies[0].apply_damage(100)
	if not await wait_until(func(): return death_count == 1 and not is_instance_valid(panels[0]), "the enemy death to remove its panel"):
		return
	enemies[0].apply_damage(10)
	enemies[0].apply_damage(0)
	enemies[0].apply_damage(-10)
	check(death_count == 1, "Repeated damage emitted another death at zero health")
	enemies[0].heal(10)
	enemies[0].apply_damage(10)
	check(death_count == 2, "A new positive-to-zero transition did not emit death")
	check(is_instance_valid(panels[1]) and is_instance_valid(inventory_root), "Dead enemy damage changed a sibling surface")
