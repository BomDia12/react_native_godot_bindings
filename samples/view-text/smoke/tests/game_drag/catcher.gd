extends Control

signal dropped(data: Variant)

var outgoing: Variant = null

func _get_drag_data(_at_position: Vector2) -> Variant:
	return outgoing

func _can_drop_data(_at_position: Vector2, data: Variant) -> bool:
	return data is Dictionary and data.get("type") == "item"

func _drop_data(_at_position: Vector2, data: Variant) -> void:
	dropped.emit(data)
