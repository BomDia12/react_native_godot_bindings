extends "res://gallery.gd"

var frames := 0
var stage := 0
var stage_frame := 0
var editor_id := 0
var native_editor_id := 0
var retained_text := ""
var retained_offset := 0
var initial_enemy_ids: Array[int] = []
var native_overlay: CanvasLayer
var authored_presses := 0
var overlapping_root_position := Vector2.ZERO
var overlapping_root_index := 0
var inventory: ReactNativeRootView
var failures: Array[String] = []

func fixture() -> Dictionary:
	var result = HermesRuntime.get_global("__phase6a")
	return result if result is Dictionary else {}

func action(name: String, value = null) -> void:
	HermesRuntime.call_function("__phase6aAction", [name, value])

func require_condition(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)

func target(id: String, node: Node = null) -> Control:
	if node == null:
		node = self
	if node is Control and node.get_meta("react_native_test_id", "") == id:
		return node
	for child in node.get_children():
		var found := target(id, child)
		if found != null:
			return found
	return null

func controls_of(type_name: String, node: Node = null) -> Array[Node]:
	if node == null:
		node = self
	var result: Array[Node] = []
	if node.is_class(type_name):
		result.append(node)
	for child in node.get_children():
		result.append_array(controls_of(type_name, child))
	return result

func native_editor() -> Control:
	var wrapper := target("inventory-editor")
	if wrapper == null:
		return null
	for child in wrapper.get_children():
		if child is LineEdit or child is TextEdit:
			return child
	return null

func pointer(position: Vector2, button: MouseButton, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.position = position
	event.global_position = position
	event.button_index = button
	event.pressed = pressed
	get_viewport().push_input(event)

func tap(position: Vector2) -> void:
	var press := InputEventScreenTouch.new()
	press.index = 0
	press.position = position
	press.pressed = true
	get_viewport().push_input(press)
	var release := InputEventScreenTouch.new()
	release.index = 0
	release.position = position
	release.pressed = false
	get_viewport().push_input(release)

func next_stage() -> void:
	stage += 1
	stage_frame = frames

func _process(_delta: float) -> void:
	frames += 1
	if frames > 1800:
		failures.append("Timed out at stage %d" % stage)
		finish()
		return
	if frames - stage_frame < 15:
		return
	match stage:
		0:
			inventory = get_node_or_null("Inventory")
			if target("inventory-editor") == null or fixture().get("enemies", {}).size() != 3:
				return
			for index in range(3):
				initial_enemy_ids.append(get_node("Enemy%d" % index).get_instance_id())
			require_condition(target("asset-image") is RNImageControl, "Asset image host is missing")
			require_condition(target("asset-image").texture != null, "Imported Metro texture is missing")
			require_condition(target("data-image").texture != null, "Base64 texture is missing")
			require_condition("asset-load" in fixture().get("events", []), "Asset onLoad was not delivered")
			require_condition("data-load" in fixture().get("events", []), "Base64 onLoad was not delivered")
			var editor := native_editor()
			editor_id = target("inventory-editor").get_instance_id()
			native_editor_id = editor.get_instance_id()
			require_condition(editor is LineEdit and editor.text == "Uncontrolled 😀", "Upstream TextInput defaultValue did not mount")
			require_condition(target("controlled-editor").get_child(0).has_focus(), "TextInput autoFocus did not run after initial publication")
			editor.grab_focus()
			editor.set_caret_column(editor.text.length())
			for character in " edited":
				var key := InputEventKey.new()
				key.pressed = true
				key.unicode = character.unicode_at(0)
				get_viewport().push_input(key)
			retained_text = editor.text
			require_condition(retained_text == "Uncontrolled 😀 edited", "Native key input was duplicated or lost")
			var list := target("inventory-list") as ScrollContainer
			require_condition(list != null, "FlatList ScrollView host is missing")
			list.scroll_vertical = 100
			retained_offset = list.scroll_vertical
			action("switch-pointer-events", "none")
			action("rerender")
			action("image-size")
			next_stage()
		1:
			require_condition(native_editor().has_focus(), "Unrelated publication repeated TextInput autoFocus")
			require_condition(target("inventory-editor").get_instance_id() == editor_id, "TextInput wrapper remounted")
			require_condition(native_editor().get_instance_id() == native_editor_id and native_editor().text == retained_text, "Uncontrolled input changed after rerender: %s versus %s" % [native_editor().text, retained_text])
			require_condition(fixture().get("uncontrolled") == retained_text, "Native editing event was not observed by React")
			require_condition((target("inventory-list") as ScrollContainer).scroll_vertical == retained_offset, "Scroll offset was not retained")
			require_condition(fixture().get("imageSize", {}).get("width", 0) > 0, "Image.getSize did not complete")
			action("scroll", 50)
			(target("section-list") as ScrollContainer).scroll_vertical = 100
			var toggle := target("settings-switch") as CheckButton
			var switch_point := toggle.get_global_rect().get_center()
			pointer(switch_point, MOUSE_BUTTON_LEFT, true)
			pointer(switch_point, MOUSE_BUTTON_LEFT, false)
			tap(switch_point)
			require_condition(not toggle.button_pressed and not toggle.disabled, "Pointer-disabled Switch toggled or changed its enabled visuals")
			require_condition(not ("switch-true" in fixture().get("events", [])), "Pointer-disabled Switch published a native change")
			action("switch-pointer-events", "auto")
			action("controlled", "Changed controlled value")
			next_stage()
		2:
			require_condition((target("inventory-list") as ScrollContainer).scroll_vertical > 1000, "FlatList.scrollToIndex did not scroll the native control")
			require_condition(fixture().get("renderedItems", []).has("50"), "FlatList did not virtualize the new window")
			require_condition(fixture().get("viewable", []).has("50"), "FlatList viewability did not follow native offsets")
			var section_header := target("section-header-Equipment")
			require_condition(absf(section_header.get_global_position().y - target("section-list").get_global_position().y) < 4.0, "SectionList header did not stick to its native viewport")
			require_condition(not ("switch-true" in fixture().get("events", [])), "Pointer-disabled Switch published a delayed change")
			var switch_point := target("settings-switch").get_global_rect().get_center()
			tap(switch_point)
			require_condition(target("controlled-editor").get_child(0).text == "Changed controlled value", "Controlled TextInput did not reconcile")
			action("mode", true)
			next_stage()
		3:
			require_condition(fixture().get("events", []).count("switch-true") == 1, "Pointer-enabled Switch did not publish exactly one native change")
			require_condition(target("inventory-editor").get_instance_id() == editor_id, "Multiline change remounted the outer input")
			require_condition(native_editor() is TextEdit and native_editor().text == retained_text, "Multiline replacement lost uncontrolled text")
			action("clear")
			action("modal", true)
			next_stage()
		4:
			require_condition(native_editor().text == "", "TextInput.clear did not dispatch the upstream command")
			require_condition("modal-show" in fixture().get("events", []), "Modal show was not published")
			require_condition(controls_of("RNModalControl").size() == 1, "Modal host is missing")
			action("nested", true)
			next_stage()
		5:
			require_condition("nested-show" in fixture().get("events", []), "Nested modal did not show")
			require_condition(controls_of("RNModalControl").size() == 2, "Nested modal host is missing")
			action("nested", false)
			next_stage()
		6:
			action("modal", false)
			next_stage()
		7:
			require_condition("modal-dismiss" in fixture().get("events", []), "Modal dismiss was not published")
			require_condition(controls_of("RNModalControl").is_empty(), "Hidden modal retained its presentation")
			var context := target("context-target")
			var point := context.get_global_rect().get_center()
			pointer(point, MOUSE_BUTTON_MIDDLE, true)
			pointer(point, MOUSE_BUTTON_MIDDLE, false)
			pointer(point, MOUSE_BUTTON_RIGHT, true)
			pointer(point, MOUSE_BUTTON_RIGHT, false)
			next_stage()
		8:
			require_condition("middle" in fixture().get("events", []), "Middle callback was not delivered")
			require_condition("right" in fixture().get("events", []), "Right callback was not delivered")
			require_condition(not ("press" in fixture().get("events", [])), "Secondary button activated Pressable")
			var menus := controls_of("PopupMenu", target("context-target"))
			for menu in menus:
				if menu.get_item_count() == 2:
					menu.emit_signal("id_pressed", 0)
					menu.hide()
			action("window", true)
			next_stage()
		9:
			require_condition("menu-inspect" in fixture().get("events", []), "Context menu stable ID was not delivered")
			var windows := controls_of("RNWindowControl")
			require_condition(windows.size() == 1, "GodotWindow host is missing")
			if not windows.is_empty():
				require_condition(windows[0].get_child(0).visible, "GodotWindow native Window is hidden")
			var window_editor := target("window-editor").get_child(0) as LineEdit
			window_editor.grab_focus()
			window_editor.set_caret_column(window_editor.text.length())
			var key := InputEventKey.new()
			key.pressed = true
			key.unicode = "!".unicode_at(0)
			window_editor.get_viewport().push_input(key)
			require_condition(window_editor.text == "Window editor!", "Native Window key input was duplicated or lost")
			action("window-modal", true)
			action("enemy", {"rootTag": get_node("Enemy1").get_root_tag(), "health": 45})
			next_stage()
		10:
			for index in range(3):
				require_condition(get_node("Enemy%d" % index).get_instance_id() == initial_enemy_ids[index], "Sibling enemy surface remounted")
			var health_values: Array = fixture().get("enemies", {}).values()
			require_condition(health_values.count(100.0) == 2 and health_values.count(45.0) == 1, "Independent presentation state reached the wrong surface: %s" % [health_values])
			require_condition(is_equal_approx(target("health", get_node("Enemy1")).size.x, 81.0), "Enemy presentation did not apply the independent health state")
			require_condition(fixture().get("windowText") == "Window editor!", "Native Window editing did not reach its React surface")
			require_condition("window-modal-show" in fixture().get("events", []), "Window-scoped Modal did not show")
			var window_content := target("window-modal-content")
			require_condition(window_content.get_viewport() is Window and window_content.get_viewport() != get_viewport(), "Modal did not bind to its nearest native Window")
			require_condition(window_content.size.is_equal_approx(Vector2(480, 320)), "Window Modal used the root's presentation bounds: %s" % window_content.size)
			action("window-modal", false)
			action("window", false)
			native_overlay = CanvasLayer.new()
			native_overlay.layer = 5
			add_child(native_overlay)
			var panel := Panel.new()
			panel.mouse_filter = Control.MOUSE_FILTER_STOP
			native_overlay.add_child(panel)
			panel.position = target("context-target").get_global_rect().position
			panel.size = target("context-target").size
			panel.gui_input.connect(func(event: InputEvent) -> void:
				if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
					authored_presses += 1
			)
			next_stage()
		11:
			var point := target("context-target").get_global_rect().get_center()
			pointer(point, MOUSE_BUTTON_LEFT, true)
			pointer(point, MOUSE_BUTTON_LEFT, false)
			next_stage()
		12:
			require_condition(authored_presses == 1, "Authored CanvasLayer did not receive native input")
			require_condition(not ("press" in fixture().get("events", [])), "React input crossed an authored CanvasLayer")
			native_overlay.queue_free()
			next_stage()
		13:
			var enemy := get_node("Enemy2") as ReactNativeRootView
			overlapping_root_position = enemy.position
			overlapping_root_index = enemy.get_index()
			enemy.position = target("context-target").get_global_rect().get_center() - Vector2(8, 8)
			move_child(enemy, get_child_count() - 1)
			next_stage()
		14:
			var point := target("context-target").get_global_rect().get_center()
			pointer(point, MOUSE_BUTTON_LEFT, true)
			pointer(point, MOUSE_BUTTON_LEFT, false)
			next_stage()
		15:
			require_condition(not ("press" in fixture().get("events", [])), "React input crossed a later overlapping React root")
			var enemy := get_node("Enemy2") as ReactNativeRootView
			enemy.position = overlapping_root_position
			move_child(enemy, overlapping_root_index)
			next_stage()
		16:
			var point := target("context-target").get_global_rect().get_center()
			pointer(point, MOUSE_BUTTON_LEFT, true)
			pointer(point, MOUSE_BUTTON_LEFT, false)
			var button_point := target("refresh-button").get_global_rect().get_center()
			pointer(button_point, MOUSE_BUTTON_LEFT, true)
			pointer(button_point, MOUSE_BUTTON_LEFT, false)
			next_stage()
		17:
			require_condition(fixture().get("events", []).count("press") == 1, "Native primary input did not activate Pressable exactly once")
			require_condition(fixture().get("events", []).count("refresh") == 1, "Native primary input did not activate Button exactly once")
			finish()

func finish() -> void:
	set_process(false)
	if failures.is_empty():
		print("RN_SMOKE_OK: game-component-contracts")
		get_tree().quit(0)
	else:
		for message in failures:
			printerr("RN_SMOKE_FAILED: game-component-contracts: " + message)
		get_tree().quit(1)
