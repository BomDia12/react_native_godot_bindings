extends Control

func _ready() -> void:
	for index in range(3):
		var enemy := ReactNativeRootView.new()
		enemy.name = "Enemy%d" % index
		enemy.application_key = "EnemyPanel"
		enemy.position = Vector2(12 + index * 200, 12)
		enemy.size = Vector2(180, 20)
		add_child(enemy)
	var inventory := ReactNativeRootView.new()
	inventory.name = "Inventory"
	inventory.application_key = "Inventory"
	inventory.position = Vector2(0, 44)
	inventory.size = size - Vector2(0, 44)
	add_child(inventory)
