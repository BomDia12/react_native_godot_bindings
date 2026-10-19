extends Control

const CHANNEL_TOLERANCE := 4.0 / 255
const PIXEL_RATIO_TOLERANCE := 0.002
const WAIT_MSEC := 30000

@export var application := ""
@export var right_to_left := false

var root: ReactNativeRootView
var output_dir := ""

func _ready() -> void:
	ReactNativeFileSingleton.set_monitored_file("res://dist/fidelity.bundle.js")
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--visual-output="):
			output_dir = argument.trim_prefix("--visual-output=")
	DirAccess.make_dir_recursive_absolute(output_dir)
	if right_to_left:
		ProjectSettings.set_setting("react_native/i18n/force_rtl", true)
	root = ReactNativeRootView.new()
	root.application_key = application
	if right_to_left:
		root.layout_direction = Control.LAYOUT_DIRECTION_RTL
	add_child(root)
	root.size = Vector2(480, 360)
	root.position = Vector2.ZERO
	_run.call_deferred()

func _run() -> void:
	var failure := await _settle()
	if failure.is_empty():
		failure = compare(_capture(), "res://smoke/visual/goldens/%s.png" % application)
	if failure.is_empty():
		print("RN_SMOKE_OK: visual-", application)
	else:
		push_error(failure)
	get_tree().quit(0 if failure.is_empty() else 1)

# Waits until React has mounted the screen and two consecutive frames match.
func _settle() -> String:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline and not await ReactNativeHost.evaluate("globalThis.__fidelity?.%s === true" % application):
		await get_tree().process_frame
	var previous: Image = null
	while Time.get_ticks_msec() < deadline:
		await RenderingServer.frame_post_draw
		var current := _capture()
		if previous != null and current.get_data() == previous.get_data():
			return ""
		previous = current
	return "%s did not settle" % application

func _capture() -> Image:
	return get_viewport().get_texture().get_image().get_region(Rect2i(root.get_global_rect()))

func compare(actual: Image, golden_path: String) -> String:
	if OS.get_cmdline_user_args().has("--visual-update"):
		actual.save_png(ProjectSettings.globalize_path(golden_path))
		return ""
	if not FileAccess.file_exists(golden_path):
		actual.save_png(output_dir.path_join(golden_path.get_file()))
		return "%s is missing" % golden_path.get_file()
	var golden := Image.load_from_file(golden_path)
	golden.convert(actual.get_format())
	if golden.get_size() != actual.get_size():
		actual.save_png(output_dir.path_join(golden_path.get_file()))
		return "%s is %s, expected %s" % [golden_path.get_file(), actual.get_size(), golden.get_size()]
	var differing := 0
	for y in actual.get_height():
		for x in actual.get_width():
			var a := actual.get_pixel(x, y)
			var b := golden.get_pixel(x, y)
			if maxf(maxf(absf(a.r - b.r), absf(a.g - b.g)), maxf(absf(a.b - b.b), absf(a.a - b.a))) > CHANNEL_TOLERANCE:
				differing += 1
	if differing > actual.get_width() * actual.get_height() * PIXEL_RATIO_TOLERANCE:
		actual.save_png(output_dir.path_join(golden_path.get_file()))
		return "%s differs in %d pixels" % [golden_path.get_file(), differing]
	return ""
