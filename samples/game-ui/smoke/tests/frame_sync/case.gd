extends "res://smoke/game_test.gd"

var expected := 0
var revisions: Array[int] = []
func _process(_delta: float) -> void:
	if not advance(): return
	if stage == 0:
		expected += 1
		enemies[0].apply_damage(1)
		stage = 1
		stage_frame = frames
	elif stage == 1:
		var observed: Dictionary = fixture().enemies[str(panels[0].get_root_tag())]
		check(observed.revision == expected and observed.health == 100 - expected, "Frame change exceeded the declared six-frame tolerance")
		revisions.append(observed.revision)
		if expected == 20:
			for index in range(revisions.size()):
				check(revisions[index] == index + 1, "Signal revisions arrived out of order")
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotHTTP').stats().started") == 0, "Offline synchronization leased a network request")
			check(fixture().network.is_empty(), "Frame synchronization made a network call")
			finish("game-frame-sync")
		else:
			stage = 0
