extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "native-animated"

func find_view(node: Node, test_id: String) -> Control:
	if node is Control and node.get_meta("react_native_test_id", "") == test_id:
		return node
	for child in node.get_children():
		var found := find_view(child, test_id)
		if found != null:
			return found
	return null

func translation(view: Control) -> Vector2:
	return view.get_offset_transform_position() if view.is_offset_transform_enabled() else Vector2()

func background(view: Control) -> Color:
	var style := view.get_theme_stylebox("panel") as RNStyleBox
	return style.get_bg_color() if style != null else Color(0, 0, 0, 0)

func validate_smoke():
	var box := find_view(mount_container(), "native-animated-box")
	if box == null:
		return "the Animated.View did not mount"
	await js("__godotNativeAnimatedStart()")
	ReactNativeHost.evaluate("(()=>{const end=Date.now()+1000;while(Date.now()<end){}return 'done';})()")
	# Commits made before the busy loop started still land in the first frames.
	await get_tree().create_timer(0.2).timeout
	var positions: Array[float] = []
	var start_msec := Time.get_ticks_msec()
	while Time.get_ticks_msec() - start_msec < 500:
		await get_tree().process_frame
		if positions.is_empty() or translation(box).x != positions.back():
			positions.append(translation(box).x)
	if positions.size() < 3:
		return "the View moved through only %d positions while JS was busy: %s" % [positions.size(), positions]
	for i in range(1, positions.size()):
		if positions[i] < positions[i - 1]:
			return "the View moved backwards: %s" % [positions]
	for i in TIMEOUT_FRAMES:
		if await js("__godotNativeAnimated.finished"):
			break
		await get_tree().process_frame
	if not await js("__godotNativeAnimated.finished"):
		return "the native animation never reported completion"
	var frame_callbacks = await js("__godotNativeAnimated.frameCallbacks")
	if frame_callbacks != 0:
		return "the native animation ran %s JS frame callbacks" % [frame_callbacks]
	if not translation(box).is_equal_approx(Vector2(120, 0)):
		return "the value animation ended at %s instead of (120, 0)" % [translation(box)]
	var tinted := find_view(mount_container(), "native-animated-tinted")
	if not translation(tinted).is_equal_approx(Vector2(60, 30)):
		return "the XY animation ended at %s instead of (60, 30)" % [translation(tinted)]
	if not background(tinted).is_equal_approx(Color(0, 0, 1)):
		return "the color animation ended at %s instead of blue" % [background(tinted)]
	await js("__godotNativeAnimatedRecolor()")
	var recolored := Color("#0088ff")
	for i in TIMEOUT_FRAMES:
		box = find_view(mount_container(), "native-animated-box")
		if box != null and background(box).is_equal_approx(recolored):
			break
		await get_tree().process_frame
	if box == null or not background(box).is_equal_approx(recolored):
		return "the commit after the animation never reached the View"
	if not translation(box).is_equal_approx(Vector2(120, 0)):
		return "the commit after the animation moved the View to %s" % [translation(box)]
	tinted = find_view(mount_container(), "native-animated-tinted")
	if not translation(tinted).is_equal_approx(Vector2(60, 30)) or not background(tinted).is_equal_approx(Color(0, 0, 1)):
		return "the commit after the animation reset the XY and color values"
	return ""
