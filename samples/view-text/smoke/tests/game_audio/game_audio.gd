extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-audio"

var failed := false

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func done() -> void:
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func _ready() -> void:
	_run.call_deferred()

func players() -> Array[Node]:
	var services: Node = null
	for child in get_tree().root.get_children(true):
		if child.name == "RNGameServices":
			services = child
	var found: Array[Node] = []
	if services:
		for child in services.get_children(true):
			if child is AudioStreamPlayer:
				found.append(child)
	return found

func _run() -> void:
	if not await wait_until(func(): return await js("__godotAudio.renders > 0"), "the app to mount"):
		return
	await js("__godotAudio.loaded = false; __godotAudio.GodotAudio.preload(['res://smoke/tests/game_audio/click.tres']).then(() => { __godotAudio.loaded = true; })")
	if not await wait_until(func(): return await js("__godotAudio.loaded"), "the preload"):
		return
	await js("__godotAudio.GodotAudio.play('res://smoke/tests/game_audio/click.tres', {volumeDb: -6, pitchScale: 1.5})")
	if not await wait_until(func(): return players().size() == 1, "the player"):
		return
	var player: AudioStreamPlayer = players()[0]
	if not is_equal_approx(player.volume_db, -6.0) or not is_equal_approx(player.pitch_scale, 1.5) or player.max_polyphony != 8 or player.process_mode != Node.PROCESS_MODE_INHERIT or player.get_parent().process_mode != Node.PROCESS_MODE_ALWAYS:
		fail("the player was not configured: %s %s %s" % [player.volume_db, player.pitch_scale, player.max_polyphony])
		return
	await js("__godotAudio.GodotAudio.play('res://smoke/tests/game_audio/click.tres', {volumeDb: -6, pitchScale: 1.5})")
	await get_tree().create_timer(0.1).timeout
	if players().size() != 1:
		fail("a second play of one sound made a second player")
		return

	await js("__godotAudio.music.setVolume(0.5)")
	if not is_equal_approx(AudioServer.get_bus_volume_linear(0), 0.5):
		fail("setVolume did not reach the bus: %s" % AudioServer.get_bus_volume_linear(0))
		return
	if not await wait_until(func(): return is_equal_approx(await js("__godotAudio.music.volume"), 0.5), "the bus hook to follow"):
		return
	await js("__godotAudio.music.setMuted(true)")
	if not AudioServer.is_bus_mute(0):
		fail("setMuted did not reach the bus")
		return
	AudioServer.set_bus_mute(0, false)
	if not await js("(() => { try { __godotAudio.GodotAudio.setBusVolume('Missing', 1); return false; } catch (error) { return error.code === 'E_VALIDATION'; } })()"):
		fail("an unknown bus did not throw E_VALIDATION")
		return
	done()
