extends Node2D

const Enemy = preload("res://game/enemy.gd")
const Inventory = preload("res://game/inventory.gd")
const HUD_WIDTH := 340.0
const POTION_ACTION := "game_potion"
const TRANSLATIONS := {
	"en": {"VOLUME": "Volume", "LANGUAGE": "Language", "REBIND": "Rebind potion key", "SLOTS": "Drag between slots"},
	"pt_BR": {"VOLUME": "Volume", "LANGUAGE": "Idioma", "REBIND": "Trocar tecla da poção", "SLOTS": "Arraste entre os espaços"},
}
var enemies: Array[Node2D] = []
var panels: Array[ReactNativeRootView] = []
var inventory: Node
var inventory_root: ReactNativeRootView
var ui: Control
var hud_root: ReactNativeRootView
var minimap: SubViewport

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
		enemy.died.connect(func(): panel.queue_free(), CONNECT_ONE_SHOT)
	inventory_root = ReactNativeRootView.new()
	inventory_root.name = "InventoryHUD"
	inventory_root.application_key = "GameInventory"
	assert(inventory_root.attach_scene_binding(inventory, inventory.binding()).is_empty())
	ui.add_child(inventory_root)
	_add_game_ui()
	get_viewport().size_changed.connect(_layout_roots)
	_layout_roots()

# The HUD, the sprite-framed inventory slots and the settings screen, in a column
# next to the inventory, over a minimap camera on the live world.
func _add_game_ui() -> void:
	for locale in TRANSLATIONS:
		var catalog := Translation.new()
		catalog.locale = locale
		for message in TRANSLATIONS[locale]:
			catalog.add_message(message, TRANSLATIONS[locale][message])
		TranslationServer.add_translation(catalog)
	if not InputMap.has_action(POTION_ACTION):
		InputMap.add_action(POTION_ACTION)
		var key := InputEventKey.new()
		key.physical_keycode = KEY_P
		InputMap.action_add_event(POTION_ACTION, key)
	minimap = SubViewport.new()
	minimap.name = "Minimap"
	minimap.size = Vector2i(160, 90)
	minimap.world_2d = get_viewport().world_2d
	minimap.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	var camera := Camera2D.new()
	camera.anchor_mode = Camera2D.ANCHOR_MODE_FIXED_TOP_LEFT
	camera.zoom = Vector2(0.25, 0.25)
	minimap.add_child(camera)
	add_child(minimap)
	ReactNativeHost.register_viewport("minimap", minimap)
	hud_root = ReactNativeRootView.new()
	hud_root.name = "GameHud"
	hud_root.application_key = "GameHud"
	ui.add_child(hud_root)
	for enemy in enemies:
		enemy.changed.connect(func(_state: Dictionary): _post_party())
	hud_root.message_received.connect(func(message: String, _payload: Variant):
		if message == "ready":
			_post_party())

func _layout_roots() -> void:
	var window := get_viewport_rect().size
	inventory_root.position = Vector2(0, 108)
	inventory_root.size = window - Vector2(HUD_WIDTH, 108)
	hud_root.position = Vector2(window.x - HUD_WIDTH, 0)
	hud_root.size = Vector2(HUD_WIDTH, window.y)

func _post_party() -> void:
	var health := 0
	var maximum := 0
	for enemy in enemies:
		health += enemy.health
		maximum += enemy.maximum_health
	hud_root.post_message("party", {"health": health, "maximum": maximum})

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode >= KEY_1 and event.keycode <= KEY_3:
			enemies[event.keycode - KEY_1].apply_damage(10)
	if event.is_action_pressed(POTION_ACTION):
		inventory.set_quantity("potion", inventory.items[0].quantity + 1)
