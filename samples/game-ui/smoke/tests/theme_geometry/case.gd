extends "res://smoke/game_test.gd"

func smoke_id() -> String:
	return "game-theme-geometry"

func dimensions() -> Dictionary:
	return (await fixture()).get("dimensions", {})

func color_scheme() -> String:
	return await js("nativeModuleProxy.Appearance.getColorScheme()")

func lifecycle() -> Node:
	for child in get_tree().root.get_children(true):
		if child.get_class() == "RNHostLifecycle":
			return child
	return null

func run() -> void:
	var original_locale := TranslationServer.get_locale()
	var original_scheme := await color_scheme()
	var original_window_size := get_window().size
	var metrics := await dimensions()
	check((await js_settled("nativeModuleProxy.LinkingManager.openURL('missing-scheme')")).get("error") == "E_VALIDATION", "Linking accepted an invalid URL")
	if DisplayServer.get_name() == "headless":
		var rejected := 0
		for expression in ["nativeModuleProxy.Clipboard.getString()", "nativeModuleProxy.Vibration.vibrate(400)", "nativeModuleProxy.LinkingManager.openURL('https://localhost')"]:
			var result = await js_settled(expression)
			rejected += 1 if result is Dictionary and result.get("error") == "E_UNSUPPORTED" else 0
		check(rejected == 3, "Unavailable host services did not reject explicitly")
	else:
		var previous = await js_settled("nativeModuleProxy.Clipboard.getString()")
		await js("nativeModuleProxy.Clipboard.setString('native clipboard')")
		check(await js_settled("nativeModuleProxy.Clipboard.getString()") == "native clipboard", "Native clipboard round trip failed")
		await js("nativeModuleProxy.Clipboard.setString(%s)" % JSON.stringify(previous if previous is String else ""))
	var text_id := target("scaled-text").get_instance_id()
	var editor_id := target("inventory-editor").get_child(0).get_instance_id()
	panels[0].position += Vector2(20, 10)
	panels[0].size = Vector2(200, 48)
	await action("scheme", ["dark"])
	ProjectSettings.set_setting("react_native/text/font_scale", 2.0)
	if not await wait_until(func(): return (await dimensions()).get("fontScale") == 2.0 and target("scaled-text").get_font_size() == 40, "the font scale to reach React and native text"):
		return
	var scaled := await dimensions()
	check(scaled.width == metrics.width and scaled.height == metrics.height, "Small root changed application Dimensions")
	check(target("scaled-text").get_instance_id() == text_id, "Font scaling replaced unchanged React text")
	check(target("inventory-editor").get_child(0).get_instance_id() == editor_id, "Metrics update replaced editor identity")
	check(target("imported-font").get_font_size() == 44, "Imported font scale was not consumed")
	check(await color_scheme() == "dark", "Explicit Appearance override did not reach native services")

	await action("scheme", ["unspecified"])
	get_tree().paused = true
	process_mode = Node.PROCESS_MODE_ALWAYS
	inventory_root.process_mode = Node.PROCESS_MODE_ALWAYS
	ProjectSettings.set_setting("react_native/text/font_scale", 1.0)
	if not await wait_until(func(): return (await dimensions()).get("fontScale") == 1.0, "real-time services while the game is paused"):
		return
	check(await color_scheme() == original_scheme, "The unspecified Appearance override did not restore the system scheme")
	lifecycle().notification(NOTIFICATION_APPLICATION_PAUSED)
	if not await wait_until(func(): return await js("nativeModuleProxy.AppState.getConstants().initialAppState") == "background", "the application pause to reach AppState"):
		return
	lifecycle().notification(NOTIFICATION_APPLICATION_RESUMED)
	get_tree().paused = false

	get_window().size = Vector2i(1200, 800)
	if not await wait_until(func(): return (await dimensions()).get("width") == get_window().get_visible_rect().size.x, "the window resize to reach public metrics"):
		return
	check((await dimensions()).width != metrics.width, "Application geometry remained a fixed bootstrap value")
	get_window().size = original_window_size

	inventory_root.layout_direction = Control.LAYOUT_DIRECTION_RTL
	if not await wait_until(func(): return target("inventory-editor").get_child(0).is_layout_rtl(), "the authored root direction to reach the native editor"):
		return
	inventory_root.layout_direction = Control.LAYOUT_DIRECTION_INHERITED
	TranslationServer.set_locale("ar")
	if not await wait_until(func(): return await js("nativeModuleProxy.I18nManager.getConstants().isRTL") == true and target("inventory-editor").get_child(0).is_layout_rtl(), "locale RTL to reach React and the native editor"):
		return
	TranslationServer.set_locale(original_locale)
