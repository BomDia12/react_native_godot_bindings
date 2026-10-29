@tool
extends EditorPlugin

const AUTOLOAD := "ScriptExampleScoreboard"

func _enable_plugin() -> void:
	add_autoload_singleton(AUTOLOAD, "res://addons/godot_script_example/scoreboard.gd")

func _disable_plugin() -> void:
	remove_autoload_singleton(AUTOLOAD)
