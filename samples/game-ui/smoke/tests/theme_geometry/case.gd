extends "res://smoke/game_test.gd"

var text_id := 0
var editor_id := 0
var metrics: Dictionary
var original_viewport: Vector2i
var original_window_size: Vector2i
var original_locale: String
var original_scheme: String
func _process(_delta: float) -> void:
	if not advance(): return
	match stage:
		0:
			original_locale = TranslationServer.get_locale()
			original_scheme = HermesRuntime.evaluate("__godotNativeModules.get('GodotServices').getState().colorScheme")
			original_viewport = get_window().content_scale_size
			original_window_size = get_window().size
			metrics = fixture().dimensions
			check(HermesRuntime.evaluate("(()=>{const service=__godotNativeModules.get('GodotServices');try{service.openURL('missing-scheme');return false;}catch(e){return e.code==='E_VALIDATION';}})()"), "Linking accepted an invalid URL")
			if DisplayServer.get_name() == "headless":
				check(HermesRuntime.evaluate("(()=>{const service=__godotNativeModules.get('GodotServices');let rejected=0;for(const action of [()=>service.clipboardGet(),()=>service.vibrate(400),()=>service.openURL('https://localhost')]){try{action();}catch(e){if(e.code==='E_UNSUPPORTED')rejected++;}}return rejected===3;})()"), "Unavailable host services did not reject explicitly")
			else:
				check(HermesRuntime.evaluate("(()=>{const service=__godotNativeModules.get('GodotServices');const old=service.clipboardGet();service.clipboardSet('native clipboard');const okay=service.clipboardGet()==='native clipboard';service.clipboardSet(old);return okay;})()"), "Native clipboard round trip failed")
			text_id = target("scaled-text").get_instance_id()
			editor_id = target("inventory-editor").get_child(0).get_instance_id()
			panels[0].position += Vector2(20, 10)
			panels[0].size = Vector2(200, 48)
			action("scheme", ["dark"])
			action("fontScale", [2.0])
			next_stage()
		1:
			check(fixture().dimensions.fontScale == 2.0, "Public fontScale did not update")
			check(fixture().dimensions.width == metrics.width and fixture().dimensions.height == metrics.height, "Small root changed application Dimensions")
			check(target("scaled-text").get_instance_id() == text_id and target("scaled-text").get_theme_font_size("normal_font_size") == 40, "Unchanged React text did not remeasure with native font scaling")
			check(target("inventory-editor").get_child(0).get_instance_id() == editor_id, "Metrics update replaced editor identity")
			check(target("imported-font").get_theme_font_size("normal_font_size") == 44, "Imported font scale was not consumed")
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotServices').getState().colorScheme") == "dark", "Explicit Appearance override did not reach native services")
			action("scheme", [null])
			get_tree().paused = true
			process_mode = Node.PROCESS_MODE_ALWAYS
			inventory_root.process_mode = Node.PROCESS_MODE_ALWAYS
			action("fontScale", [1.0])
			next_stage()
		2:
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotServices').getState().colorScheme") == original_scheme, "Null Appearance override did not restore the system scheme")
			check(fixture().dimensions.fontScale == 1.0, "Real-time services stopped while game was paused")
			inventory_root.notification(Node.NOTIFICATION_APPLICATION_PAUSED)
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotServices').getState().initialAppState") == "background", "Application pause did not reach AppState")
			inventory_root.notification(Node.NOTIFICATION_APPLICATION_RESUMED)
			get_tree().paused = false
			get_window().size = Vector2i(1200, 800)
			next_stage()
		3:
			check(fixture().dimensions.width == get_window().get_visible_rect().size.x, "Window resize did not reach public metrics")
			check(fixture().dimensions.width != metrics.width, "Application geometry remained a fixed bootstrap value")
			get_window().size = original_window_size
			inventory_root.layout_direction = Control.LAYOUT_DIRECTION_RTL
			next_stage()
		4:
			check(target("scaled-text").is_layout_rtl(), "Authored root direction did not reach native text")
			inventory_root.layout_direction = Control.LAYOUT_DIRECTION_INHERITED
			TranslationServer.set_locale("ar")
			next_stage()
		5:
			check(HermesRuntime.evaluate("__godotNativeModules.get('GodotServices').getState().direction.isRTL") and target("scaled-text").is_layout_rtl(), "Locale RTL did not invalidate the unchanged native tree")
			TranslationServer.set_locale(original_locale)
			finish("game-theme-geometry")
