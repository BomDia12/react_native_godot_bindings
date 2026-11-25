@tool
extends EditorPlugin

func _enter_tree() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--editor-smoke="):
			var smoke: Node = load(argument.trim_prefix("--editor-smoke=")).new()
			add_child(smoke)
