extends "res://smoke/game_test.gd"

const SETTINGS := "user://react_native/async_storage/game-settings.json"

func smoke_id() -> String:
	return "game-hud"

func _init() -> void:
	DirAccess.remove_absolute(SETTINGS)

func press(physical: Key) -> void:
	var key := InputEventKey.new()
	key.physical_keycode = physical
	key.keycode = physical
	key.pressed = true
	get_viewport().push_input(key)
	var release: InputEventKey = key.duplicate()
	release.pressed = false
	get_viewport().push_input(release)

func hud_control(native_id: String) -> Node:
	return hud_root.get_native_node(native_id)

func run() -> void:
	if not await wait_until(func(): return hud_control("volume") != null and await js("__gameHud.party.maximum === 300 && __gameHud.restored === true"), "the HUD and saved settings"):
		return
	var bar := hud_control("party-health") as ProgressBar
	check(is_equal_approx(bar.value, 300.0), "Party health started at %s" % bar.value)
	enemies[0].apply_damage(40)
	if not await wait_until(func(): return is_equal_approx(bar.value, 260.0), "party health to follow the enemy"):
		return
	var ring := hud_control("party-ring") as TextureProgressBar
	if not await wait_until(func(): return ring.texture_progress != null, "the radial bar's textures"):
		return

	var map := hud_control("minimap") as TextureRect
	if not await wait_until(func(): return map.texture == minimap.get_texture(), "the minimap to show its viewport"):
		return
	check(minimap.world_2d == get_viewport().world_2d, "The minimap does not share the game's world")
	check((hud_control("party") as Control).get_tooltip(Vector2.ZERO) == "The health of every enemy", "The HUD lost its tooltip")

	var slot := hud_control("slot-0") as Panel
	check(slot.get_theme_stylebox("panel") == load("res://game/ui/frame.tres"), "A slot is not sprite framed")
	slot.force_drag({"type": "item", "from": 0, "name": "Sword"}, null)
	await frames(2)
	get_viewport().gui_cancel_drag()
	await frames(2)

	await js("__gameHud.setVolume(0.5)")
	if not await wait_until(func(): return is_equal_approx(AudioServer.get_bus_volume_linear(0), 0.5), "the volume to reach the bus"):
		return
	await js("__gameHud.changeLocale('pt_BR')")
	if not await wait_until(func(): return await js("__gameHud.languageLabel === 'Idioma'"), "the language to change"):
		return
	await js("globalThis.__rebinding = __gameHud.rebind()")
	await get_tree().create_timer(0.1).timeout
	press(KEY_O)
	if not await wait_until(func():
			var events := InputMap.action_get_events(POTION_ACTION)
			return events.size() == 1 and (events[0] as InputEventKey).physical_keycode == KEY_O, "the potion key to rebind"):
		return
	var potions: int = inventory.items[0].quantity
	press(KEY_O)
	await frames(2)
	check(inventory.items[0].quantity == potions + 1, "The rebound key did not pick up a potion")
	await get_tree().create_timer(0.2).timeout

	# A new runtime restores what the settings saved.
	AudioServer.set_bus_volume_linear(0, 1.0)
	TranslationServer.set_locale("en")
	InputMap.action_erase_events(POTION_ACTION)
	var generation := ReactNativeHost.get_generation()
	await js("__gameHud.restored = false")
	ReactNativeHost.reload()
	if not await wait_until(func(): return ReactNativeHost.get_generation() != generation and await js("globalThis.__gameHud?.restored === true"), "the settings after a reload"):
		return
	check(is_equal_approx(AudioServer.get_bus_volume_linear(0), 0.5), "The volume was not restored")
	check(TranslationServer.get_locale() == "pt_BR", "The language was not restored")
	check(InputMap.action_get_events(POTION_ACTION).size() == 1, "The binding was not restored")
	AudioServer.set_bus_volume_linear(0, 1.0)
	TranslationServer.set_locale("en")
	# Quitting while the new runtime still mounts can leave a font behind at exit.
	await wait_until(game_ready, "the game UI after the reload")
	await frames(30)
