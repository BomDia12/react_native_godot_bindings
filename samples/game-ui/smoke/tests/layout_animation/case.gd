extends "res://smoke/animation_probe.gd"

func smoke_id() -> String:
	return "game-layout-animation"

func steps() -> Array:
	return await js("__animation.layout")

func run() -> void:
	if not await mount_probe():
		return
	var first := target("layout-first", probe)
	await js("__animation.layoutStep('update')")
	var widths: Array[float] = []
	while not (await steps()).has("update") and widths.size() < 600:
		widths.append(first.size.x)
		await get_tree().process_frame
	check(widths.any(func(width): return width > 41 and width < 119), "LayoutAnimation did not interpolate the width: %s" % [widths])
	if not await wait_until(func(): return first.size.x == 120, "the update to reach its final width"):
		return

	await js("__animation.layoutStep('create')")
	if not await wait_until(func(): return target("layout-third", probe) != null, "the created view to mount"):
		return
	var third := target("layout-third", probe)
	var opacities: Array[float] = []
	while not (await steps()).has("create") and opacities.size() < 600:
		opacities.append(third.modulate.a)
		await get_tree().process_frame
	check(opacities.any(func(alpha): return alpha > 0.01 and alpha < 0.99), "LayoutAnimation did not fade the created view in: %s" % [opacities])

	var first_id := first.get_instance_id()
	await js("__animation.layoutStep('delete')")
	await frames(5)
	check(is_instance_id_valid(first_id) and first.is_inside_tree(), "The deleted view left before its animation ended")
	if not await wait_until(func(): return (await steps()).has("delete"), "the delete animation to complete"):
		return
	if not await wait_until(func(): return not is_instance_id_valid(first_id) or not (instance_from_id(first_id) as Node).is_inside_tree(), "the deleted Godot control to leave after its animation"):
		return
	check(await steps() == ["update", "create", "delete"], "Completion callbacks ran out of order: %s" % [await steps()])
