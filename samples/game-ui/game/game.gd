extends Node2D

const Enemy = preload("res://game/enemy.gd")
const Inventory = preload("res://game/inventory.gd")
var enemies: Array[Node2D] = []
var panels: Array[ReactNativeRootView] = []
var inventory: Node
var inventory_root: ReactNativeRootView
var ui: Control

func _ready() -> void:
	ReactNativeFileSingleton.set_monitored_file("res://dist/game.bundle.js")
	inventory = Inventory.new()
	inventory.name = "InventoryState"
	add_child(inventory)
	ui = Control.new()
	ui.name = "UIRoot"
	add_child(ui)
	for index in range(3):
		var enemy := Enemy.new()
		enemy.name = "Enemy%d" % index
		enemy.enemy_id = "enemy-%d" % index
		enemy.display_name = "Enemy %d" % (index + 1)
		enemy.position = Vector2(100 + index * 220, 64)
		add_child(enemy)
		enemies.append(enemy)
		var panel := ReactNativeRootView.new()
		panel.name = "EnemyPanel%d" % index
		panel.application_key = "EnemyStatus"
		panel.position = Vector2(12 + index * 220, 12)
		panel.size = Vector2(180, 36)
		assert(panel.attach_scene_binding(enemy, enemy.binding()).is_empty())
		ui.add_child(panel)
		panels.append(panel)
		enemy.died.connect(func(): panel.queue_free())
	inventory_root = ReactNativeRootView.new()
	inventory_root.name = "InventoryHUD"
	inventory_root.application_key = "GameInventory"
	inventory_root.position = Vector2(0, 108)
	inventory_root.size = get_viewport_rect().size - Vector2(0, 108)
	assert(inventory_root.attach_scene_binding(inventory, inventory.binding()).is_empty())
	ui.add_child(inventory_root)
	get_viewport().size_changed.connect(func(): inventory_root.size = get_viewport_rect().size - Vector2(0, 108))

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode >= KEY_1 and event.keycode <= KEY_3:
			enemies[event.keycode - KEY_1].apply_damage(10)
		elif event.keycode == KEY_P:
			inventory.set_quantity("potion", inventory.items[0].quantity + 1)
