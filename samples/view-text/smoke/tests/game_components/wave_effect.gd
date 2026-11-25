@tool
extends RichTextEffect

var bbcode := "smoke"

func _process_custom_fx(char_fx: CharFXTransform) -> bool:
	return true
