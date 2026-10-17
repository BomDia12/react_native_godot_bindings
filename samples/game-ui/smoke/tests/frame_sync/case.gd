extends "res://smoke/game_test.gd"

func smoke_id() -> String:
	return "game-frame-sync"

func observed() -> Dictionary:
	return (await fixture()).enemies.get(str(panels[0].get_root_tag()), {})

func run() -> void:
	var revisions: Array[int] = []
	for expected in range(1, 21):
		enemies[0].apply_damage(1)
		if not await wait_until(func(): return int((await observed()).get("revision", 0)) >= expected, "revision %d" % expected):
			return
		var state := await observed()
		check(int(state.revision) == expected and int(state.health) == 100 - expected, "A frame change was skipped or merged")
		revisions.append(int(state.revision))
	for index in range(revisions.size()):
		check(revisions[index] == index + 1, "Signal revisions arrived out of order")
	check(int(ReactNativeHost.get_service_stats().http.started) == 0, "Offline synchronization leased a network request")
	check((await fixture()).network.is_empty(), "Frame synchronization made a network call")
