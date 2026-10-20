extends "res://smoke/animation_probe.gd"

func smoke_id() -> String:
	return "game-animation-conformance"

func translation(view: Control) -> float:
	return view.get_offset_transform_position().x if view.is_offset_transform_enabled() else 0.0

func background(view: Control) -> Color:
	return (view.get_theme_stylebox("panel") as RNStyleBox).get_bg_color()

func run() -> void:
	if not await mount_probe():
		return
	var settled = await js_settled("__animation.conformance()")
	if settled is Dictionary and settled.has("error"):
		failures.append("Animated conformance threw %s" % settled.error)
		return
	var ends: Dictionary = await js("__animation.ends")
	var results: Dictionary = await js("__animation.results")
	for name in ends:
		if name.begins_with("timing-") or name.begins_with("spring-"):
			check(is_equal_approx(ends[name], 1.0), "%s ended at %s instead of 1" % [name, ends[name]])
	check(translation(target("native-decay", probe)) > 10, "The native decay did not move its view")
	var expected := {"sequence": 2, "parallel": 7, "stagger": 3, "loop": 1, "stop": 1, "extend": 20, "clamp": 10, "identity": 2, "diffClamp": 0}
	for name in expected:
		check(is_equal_approx(ends.get(name, -1), expected[name]), "%s ended at %s instead of %s" % [name, ends.get(name), expected[name]])
	for name in results:
		check(results[name].finished == (name != "stop"), "%s reported finished=%s" % [name, results[name].finished])

	var native := target("native-driven", probe)
	var driven := target("js-driven", probe)
	var text := target("js-text", probe) as RNTextControl
	var native_text := target("native-text", probe) as RNTextControl
	var samples: Array[Vector2] = []
	var start := Time.get_ticks_msec()
	await js("__animation.drivers()")
	while Time.get_ticks_msec() - start < 1500:
		samples.append(Vector2(Time.get_ticks_msec() - start, native.modulate.a))
		await get_tree().process_frame
	for i in range(1, samples.size()):
		check(samples[i].y <= samples[i - 1].y + 0.001, "The native driver's opacity went back up: %s" % [samples])
	# Linear timing changes opacity by 0.5 over 400 ms; frames between the first and last
	# change must fit that slope within a frame either side.
	var moving := samples.filter(func(sample): return sample.y < 0.999 and sample.y > 0.501)
	check(moving.size() >= 2, "Too few frames caught the native driver moving: %s" % [samples])
	if moving.size() >= 2:
		var slope: float = (moving[0].y - moving[-1].y) / maxf(1.0, moving[-1].x - moving[0].x)
		check(absf(slope - 0.5 / 400.0) < 0.5 / 400.0 * 0.5, "The native driver moved at %s per ms instead of %s" % [slope, 0.5 / 400.0])
	if not await wait_until(func(): return (await js("__animation.results")).has("native-driver") and (await js("__animation.results")).has("js-driver"), "both drivers to finish"):
		return
	await frames(3)
	check(is_equal_approx(native.modulate.a, 0.5) and is_equal_approx(translation(native), 30), "The native driver ended at %s, %s" % [native.modulate.a, translation(native)])
	check(is_equal_approx(driven.modulate.a, 0.5) and is_equal_approx(translation(driven), 30), "The JS driver ended at %s, %s" % [driven.modulate.a, translation(driven)])
	check(background(driven).is_equal_approx(Color(0, 0, 1)), "The JS driver's background ended at %s" % background(driven))
	check(text.get_text_color().is_equal_approx(Color(0, 0, 1)), "Animated Text color ended at %s" % text.get_text_color())
	check(background(native).is_equal_approx(Color(0, 0, 1)), "The native driver's background ended at %s" % background(native))
	check(native_text.get_text_color().is_equal_approx(Color(0, 0, 1)), "Native-driven Text color ended at %s" % native_text.get_text_color())

	var scroll := target("animated-scroll", probe) as ScrollContainer
	scroll.scroll_vertical = 80
	if not await wait_until(func(): return await js("__animation.scrollValue()") == scroll.scroll_vertical, "Animated.event to map the scroll offset into its value"):
		return
