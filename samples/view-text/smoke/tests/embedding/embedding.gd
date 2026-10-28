extends Node

const WAIT_MSEC := 20000

var failed := false

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: embedding: %s" % message)
		get_tree().quit(1)

func layouts() -> Dictionary:
	var value = await ReactNativeHost.evaluate("__godotEmbedding")
	return value if value is Dictionary else {}

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	var fitted: ReactNativeRootView = $Column/Fitted
	if not await wait_until(func(): return (await layouts()).size() == 4 and is_equal_approx(fitted.size.y, 48.0), "every root to lay out"):
		return
	await get_tree().process_frame
	var below: Label = $Column/Below
	if below.position.y < 48.0:
		fail("the container placed the next control at %s, inside the fitted root" % below.position.y)
		return
	var measured := await layouts()
	var normal_width: float = measured["normal"]["textWidth"]
	var wide_width: float = measured["wide"]["textWidth"]
	if wide_width < normal_width * 1.3:
		fail("the themed root measured text %s wide against %s" % [wide_width, normal_width])
		return
	var inner: Dictionary = measured["inner"]
	if not is_equal_approx(inner["windowX"], 400.0) or not is_equal_approx(inner["windowY"], 300.0):
		fail("a root in a SubViewport measured itself at %s,%s in the window" % [inner["windowX"], inner["windowY"]])
		return
	print("RN_SMOKE_OK: embedding")
	get_tree().quit(0)
