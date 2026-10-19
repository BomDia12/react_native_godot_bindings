extends "res://smoke/game_test.gd"

var probe: ReactNativeRootView

# Mounts the AnimationProbe application in its own root beside the game.
func mount_probe() -> bool:
	# The headless window starts too small to lay the probe out.
	get_window().size = Vector2i(1024, 768)
	# Headless frames are otherwise uncapped and far shorter than a display's,
	# which ends a decay on its first frame.
	Engine.max_fps = 60
	probe = ReactNativeRootView.new()
	probe.application_key = "AnimationProbe"
	probe.position = Vector2(400, 300)
	probe.size = Vector2(600, 400)
	ui.add_child(probe)
	return await wait_until(func(): return target("layout-first", probe) != null and await js("typeof __animation?.layoutStep") == "function", "the animation probe to mount")
