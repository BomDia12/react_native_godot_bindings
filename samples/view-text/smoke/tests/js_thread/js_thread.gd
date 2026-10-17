extends "res://smoke/support/smoke_case.gd"

func smoke_id() -> String:
	return "js-thread"

func counter_binding() -> RNSceneBinding:
	var binding := RNSceneBinding.new()
	binding.capability = "ExampleCounter"
	binding.snapshot_method = &"read"
	binding.snapshot_schema = {"type": "record", "fields": {"value": {"type": "number"}, "position": {"type": "Vector2"}}}
	return binding

func _ready() -> void:
	var error := attach_scene_binding($Counter, counter_binding())
	if not error.is_empty():
		fail_smoke("scene binding did not attach: %s" % [error])
		return
	super._ready()

func validate_smoke():
	var busy: Signal = ReactNativeHost.evaluate("(()=>{const end=Date.now()+250;while(Date.now()<end){}return 'done';})()")
	var start_frame := Engine.get_process_frames()
	var start_msec := Time.get_ticks_msec()
	await get_tree().create_timer(0.1).timeout
	var frames_during_busy := Engine.get_process_frames() - start_frame
	if frames_during_busy < 3:
		return "Godot advanced only %d frames while JS was busy" % frames_during_busy
	if await busy != "done":
		return "the busy JS evaluation did not complete"
	if Time.get_ticks_msec() - start_msec < 250:
		return "the busy loop finished before its deadline"
	var value = await js("(()=>{const scene=nativeModuleProxy.GodotSceneCxx;const binding=scene.getBinding(%d).binding;return scene.read(%d,binding).payload.value;})()" % [get_root_tag(), get_root_tag()])
	if value != 0.0:
		return "a synchronous GodotScene call returned %s" % [value]
	return await validate_reload()

func validate_reload():
	var old_tag := get_root_tag()
	var busy_result := ["pending"]
	ReactNativeHost.evaluate("(()=>{const end=Date.now()+3000;while(Date.now()<end){}return 'done';})()").connect(func(result): busy_result[0] = result)
	await get_tree().create_timer(1.0).timeout
	var start_msec := Time.get_ticks_msec()
	ReactNativeHost.reload()
	var reload_msec := Time.get_ticks_msec() - start_msec
	if reload_msec >= 250:
		return "reload() blocked the main thread for %d ms" % reload_msec
	var start_frame := Engine.get_process_frames()
	await get_tree().create_timer(0.1).timeout
	var frames_after_reload := Engine.get_process_frames() - start_frame
	if frames_after_reload < 3:
		return "Godot advanced only %d frames after the reload" % frames_after_reload
	await get_tree().process_frame
	if busy_result[0] != null:
		return "the evaluation on the retired runtime completed with %s instead of null" % [busy_result[0]]
	for i in TIMEOUT_FRAMES:
		if get_root_tag() != 0 and get_root_tag() != old_tag and mount_container().get_child_count() > 0:
			break
		await get_tree().process_frame
	if get_root_tag() == 0 or get_root_tag() == old_tag:
		return "the root did not remount with a new root tag"
	if mount_container().get_child_count() == 0:
		return "the remounted root rendered nothing"
	if not ReactNativeHost.get_last_error().is_empty():
		return "the retired runtime reported an error: %s" % ReactNativeHost.get_last_error()
	return ""
