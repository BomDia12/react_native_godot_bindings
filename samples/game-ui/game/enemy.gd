extends Node2D

const Schema = preload("res://game/schemas.gd")
signal changed(state: Dictionary)
signal died

@export var enemy_id := "enemy"
@export var display_name := "Enemy"
@export var maximum_health := 100
var health := 100
var last_damage := 0
var revision := 0

func _ready() -> void:
	health = maximum_health
	queue_redraw()

func _draw() -> void:
	draw_circle(Vector2.ZERO, 24, Color("bd4f4f") if health > 0 else Color("555555"))

func snapshot() -> Dictionary:
	return {"id": enemy_id, "name": display_name, "health": health,
		"maxHealth": maximum_health, "lastDamage": last_damage, "revision": revision}

func apply_damage(amount: int) -> Dictionary:
	var was_alive := health > 0
	last_damage = clampi(amount, 0, health)
	health -= last_damage
	revision += 1
	changed.emit(snapshot())
	queue_redraw()
	if was_alive and health == 0:
		died.emit()
	return snapshot()

func heal(amount: int) -> Dictionary:
	health = clampi(health + maxi(0, amount), 0, maximum_health)
	last_damage = 0
	revision += 1
	changed.emit(snapshot())
	queue_redraw()
	return snapshot()

func binding() -> RNSceneBinding:
	var state := Schema.record({"id": Schema.value("string"), "name": Schema.value("string"),
		"health": Schema.value("integer"), "maxHealth": Schema.value("integer"),
		"lastDamage": Schema.value("integer"), "revision": Schema.value("integer")})
	return Schema.binding("EnemyStatus", state, {
		"damage": Schema.command("apply_damage", [Schema.argument("amount", Schema.value("integer"))], state),
		"heal": Schema.command("heal", [Schema.argument("amount", Schema.value("integer"))], state)})
