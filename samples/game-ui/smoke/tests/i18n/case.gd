extends "res://smoke/game_test.gd"

func smoke_id() -> String:
	return "game-i18n"

func click(position: Vector2) -> void:
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.device = 1
		event.position = position
		event.global_position = position
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		get_viewport().push_input(event)
		await get_tree().process_frame

func events() -> Array:
	return (await fixture()).get("events", [])

func run() -> void:
	await checks()
	DirAccess.remove_absolute(ProjectSettings.globalize_path(DIRECTION_PREFERENCES))

func checks() -> void:
	# The headless window starts too small to lay the inventory out.
	get_window().size = Vector2i(1024, 768)
	var bidi := target("bidi-text")
	if not await wait_until(func(): return bidi.size.y > 0 and target("asset-image").get_global_rect().position.y > bidi.get_global_rect().end.y, "the inventory to lay out in the window"):
		return
	var rect := bidi.get_global_rect()
	await click(Vector2(rect.end.x - 4, rect.get_center().y))
	if not await wait_until(func(): return (await events()).has("arabic-press"), "the Arabic span to receive its own press"):
		return
	check(target("asset-image").get_global_rect().position.x < inventory_root.get_global_rect().get_center().x, "The left-to-right layout started on the right")

	await action("forceRTL", [true])
	ReactNativeHost.reload()
	if not await wait_until(func(): return await js("nativeModuleProxy.I18nManager.getConstants().isRTL") == true and target("asset-image") != null and target("asset-image").get_global_rect().position.x > inventory_root.get_global_rect().get_center().x, "a forced right-to-left restart to mirror the layout"):
		return
	await action("forceRTL", [false])
