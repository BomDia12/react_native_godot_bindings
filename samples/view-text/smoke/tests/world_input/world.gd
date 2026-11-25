extends Node2D

var presses := 0
var releases := 0
var wheels := 0
var motions := 0

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
		wheels += 1
	elif event is InputEventMouseButton or event is InputEventScreenTouch:
		if event.pressed:
			presses += 1
		else:
			releases += 1
	elif event is InputEventMouseMotion:
		motions += 1

func reset() -> void:
	presses = 0
	releases = 0
	wheels = 0
	motions = 0
