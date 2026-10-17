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
	return ""
