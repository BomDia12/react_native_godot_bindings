@tool
extends Node

# Configures a root through the React Native inspector, then runs a scene whose
# JavaScript throws and reads the source-mapped error in the debugger tab.

const WAIT_FRAMES := 30 * 60
const RUN_ARGS := "editor/run/main_run_args"

func fail(message: String) -> void:
	printerr("RN_SMOKE_FAILED: editor-tools: %s" % message)
	get_tree().quit(1)

func wait_until(condition: Callable, what: String) -> bool:
	for i in WAIT_FRAMES:
		if condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % what)
	return false

func find_class(node: Node, name: String) -> Node:
	if node.get_class() == name:
		return node
	for child in node.get_children(true):
		var found := find_class(child, name)
		if found != null:
			return found
	return null

func find_property(node: Node, property: String) -> EditorProperty:
	if node is EditorProperty and node.get_edited_property() == property:
		return node
	for child in node.get_children(true):
		var found := find_property(child, property)
		if found != null:
			return found
	return null

func pick(picker: Node, component: String) -> bool:
	var options: OptionButton = find_class(picker, "OptionButton")
	for i in options.item_count:
		if options.get_item_text(i) == component:
			options.select(i)
			options.item_selected.emit(i)
			return true
	return false

func source_mapped_error(tab: Node) -> bool:
	var errors: Tree = find_class(tab, "Tree")
	for error in errors.get_root().get_children():
		if error.get_text(0).contains("Editor smoke error"):
			for frame in error.get_children():
				if frame.get_text(0).contains("gallery.entry.js:"):
					return true
	return false

func _ready() -> void:
	var filesystem := EditorInterface.get_resource_filesystem()
	# The editor starts its first file scan after it initializes plugins.
	for i in 10:
		await get_tree().process_frame
	if not await wait_until(func(): return not filesystem.is_scanning(), "the first scan"):
		return

	EditorInterface.open_scene_from_path("res://smoke/tests/editor_tools/EditorRoot.tscn")
	var root: ReactNativeRootView = EditorInterface.get_edited_scene_root()
	EditorInterface.edit_node(root)
	var inspector := EditorInterface.get_inspector()
	if not await wait_until(func(): return find_class(inspector, "RNComponentPicker") != null, "the component picker"):
		return
	if not pick(find_class(inspector, "RNComponentPicker"), "EnemyHealthBar"):
		fail("the component picker doesn't list EnemyHealthBar")
		return
	if root.application_key != "EnemyHealthBar":
		fail("picking a component set application_key to '%s'" % root.application_key)
		return
	if not await wait_until(func(): return find_property(inspector, "health") != null, "the contract properties"):
		return
	find_property(inspector, "health").emit_changed("health", 4)
	if root.get("health") != 4:
		fail("the inspector set health to %s" % root.get("health"))
		return

	# Running a scene saves the edited scenes first.
	EditorInterface.close_scene()
	var run_args = ProjectSettings.get_setting(RUN_ARGS)
	ProjectSettings.set_setting(RUN_ARGS, "--headless --audio-driver Dummy")
	await ProjectSettings.settings_changed
	EditorInterface.play_custom_scene("res://smoke/tests/editor_tools/ThrowingRoot.tscn")
	var tab := find_class(EditorInterface.get_base_control(), "RNDebuggerTab")
	var reported := tab != null and await wait_until(func(): return source_mapped_error(tab), "the source-mapped error")
	EditorInterface.stop_playing_scene()
	ProjectSettings.set_setting(RUN_ARGS, run_args)
	if tab == null:
		fail("the React Native debugger tab is missing")
		return
	if not reported:
		return
	print("RN_SMOKE_OK: editor-tools")
	get_tree().quit(0)
