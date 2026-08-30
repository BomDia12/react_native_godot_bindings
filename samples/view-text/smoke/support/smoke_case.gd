extends ReactNativeRootView

const TIMEOUT_FRAMES := 600
const SETTLE_FRAMES := 30

var frames := 0
var settle_frames := -1

func smoke_id() -> String:
	return ""

func validate_smoke() -> String:
	return "smoke test did not implement validate_smoke"

func fail_smoke(message: String) -> void:
	printerr("RN_SMOKE_FAILED: %s: %s" % [smoke_id(), message])
	get_tree().quit(1)

func _process(_delta: float) -> void:
	frames += 1

	if settle_frames < 0:
		if get_child_count() > 0:
			settle_frames = 0
		elif frames >= TIMEOUT_FRAMES:
			fail_smoke("no Fabric root commit after %d frames" % TIMEOUT_FRAMES)
		return

	settle_frames += 1
	if settle_frames < SETTLE_FRAMES:
		return

	set_process(false)
	var failure := validate_smoke()
	if not failure.is_empty():
		fail_smoke(failure)
		return

	print("RN_SMOKE_OK: %s" % smoke_id())
	get_tree().quit(0)
