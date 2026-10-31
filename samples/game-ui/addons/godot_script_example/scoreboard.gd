extends Node

## The Scoreboard module, which every React Native root reaches through
## useScoreboard() from react-native-godot-script-example.

signal score_changed(score: int)

var score := 0

func _ready() -> void:
	var error := ReactNativeHost.register_module("Scoreboard", self, preload("scoreboard_binding.tres"))
	assert(error.is_empty(), str(error))

func snapshot() -> Dictionary:
	return {"score": score}

func add(points: int) -> int:
	score += points
	score_changed.emit(score)
	return score
