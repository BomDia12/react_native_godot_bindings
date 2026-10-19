extends "res://smoke/game_test.gd"

func smoke_id() -> String:
	return "game-accessibility-info"

func run() -> void:
	var editor := target("inventory-editor").get_child(0) as Control
	await action("accessibility")
	if not await wait_until(func(): return (await fixture()).get("accessibility") is Dictionary, "AccessibilityInfo queries to resolve"):
		return
	var state: Dictionary = (await fixture()).accessibility
	check(state.screenReader == false, "A headless host reported a screen reader")
	check(state.reduceMotion == false, "Reduce motion did not resolve false")
	if not await wait_until(func(): return editor.has_focus(), "setAccessibilityFocus to focus the editor"):
		return
