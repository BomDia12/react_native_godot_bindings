extends ReactNativeRootView

const TIMEOUT_FRAMES := 600
const SETTLE_FRAMES := 30

var frames := 0

func smoke_id() -> String:
	return ""

func ready_to_validate():
	return true

func validate_smoke():
	return "smoke test did not implement validate_smoke"

func fail_smoke(message: String) -> void:
	printerr("RN_SMOKE_FAILED: %s: %s" % [smoke_id(), message])
	get_tree().quit(1)

func mount_container() -> Control:
	return get_child(0, true)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func next_frame() -> void:
	frames += 1
	await get_tree().process_frame

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	while mount_container().get_child_count() == 0:
		if frames >= TIMEOUT_FRAMES:
			fail_smoke("no React Native mount after %d frames" % TIMEOUT_FRAMES)
			return
		await next_frame()
	for i in SETTLE_FRAMES:
		await next_frame()
	while not await ready_to_validate():
		if frames >= TIMEOUT_FRAMES:
			fail_smoke("asynchronous fixture did not become ready")
			return
		await next_frame()
	var failure = await validate_smoke()
	if not String(failure).is_empty():
		fail_smoke(failure)
		return
	print("RN_SMOKE_OK: %s" % smoke_id())
	get_tree().quit(0)
