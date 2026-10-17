extends "res://smoke/game_test.gd"

func smoke_id() -> String:
	return "game-root-lifecycle"

func enemy_health(root: ReactNativeRootView) -> int:
	return int((await fixture()).enemies.get(str(root.get_root_tag()), {}).get("health", -1))

func probe_ready() -> bool:
	var probe = await js("__probe")
	return probe is Dictionary and probe.get("ready", false)

func run() -> void:
	var old_tag := panels[0].get_root_tag()
	var sibling_tag := panels[1].get_root_tag()
	var sibling_control := panels[1].get_child(0, true).get_child(0).get_instance_id()
	enemies[0].apply_damage(13)
	panels[0].reload()
	if not await wait_until(func(): return panels[0].get_root_tag() != old_tag and await enemy_health(panels[0]) == 87, "the reloaded root to render Godot state"):
		return
	check(panels[1].get_root_tag() == sibling_tag and panels[1].get_child(0, true).get_child(0).get_instance_id() == sibling_control, "Sibling identity changed during reload")

	ui.remove_child(panels[0])
	if not await wait_until(func(): return (await fixture()).enemies.size() == 2, "the departed root to unmount"):
		return
	ui.add_child(panels[0])
	if not await wait_until(func(): return await enemy_health(panels[0]) == 87, "re-entry to keep persistent state"):
		return
	var generation := ReactNativeHost.get_generation()
	ReactNativeFileSingleton.force_refresh()
	if not await wait_until(func(): return ReactNativeHost.get_generation() == generation + 1 and (await fixture()).enemies.size() == 3 and await enemy_health(panels[0]) == 87, "the bundle reset to rebuild from Godot state"):
		return
	enemies[2].apply_damage(100)
	if not await wait_until(func(): return (await fixture()).enemies.size() == 2 and enemies[2].health == 0, "the dead enemy surface to unmount"):
		return

	var probe: Node = preload("res://smoke/tests/root_lifecycle/probe.gd").new()
	var probe_root := ReactNativeRootView.new()
	probe_root.application_key = "SceneProbe"
	probe_root.size = Vector2(200, 40)
	probe.ui = probe_root
	var authored: RNSceneBinding = probe.binding()
	var loaded: RNSceneBinding = load("res://smoke/tests/root_lifecycle/probe.tres")
	check(authored.snapshot_schema == loaded.snapshot_schema and authored.commands == loaded.commands and authored.signals == loaded.signals, "GDScript and resource schemas differ")
	check(probe_root.attach_scene_binding(probe, loaded).is_empty(), "Unrelated resource schema failed to attach")
	ui.add_child(probe_root)
	if not await wait_until(func(): return probe_root.get_root_tag() != 0 and await js("globalThis.__probe !== undefined"), "the probe root to render"):
		return
	await frames(4)
	check(not await probe_ready(), "Target outside the tree became ready")
	await js("globalThis.probeModule=nativeModuleProxy.GodotSceneCxx;globalThis.probeTag=%d;globalThis.otherTag=%d;globalThis.probeEvents=[];probeModule.onChanged(e=>{if(e.rootTag===probeTag)probeEvents.push(e);});probeModule.getBinding(probeTag);undefined;" % [probe_root.get_root_tag(), panels[1].get_root_tag()])
	add_child(probe)
	if not await wait_until(probe_ready, "tree entry to publish readiness"):
		return
	check(await js("probeEvents.some(e=>e.ready)"), "Subscribe-before-read missed readiness")
	await js("globalThis.probeHandle=probeModule.getBinding(probeTag).binding;globalThis.probeResults={};try{probeModule.read(otherTag,probeHandle);}catch(e){probeResults.cross=e.code;}probeResults.adjust=probeModule.call(probeTag,probeHandle,'adjust',[]);undefined;")
	check((await js("probeResults")).get("cross") == "E_STALE_HANDLE", "Handle crossed roots")
	check(probe.temperature == 19.0, "Script optional default was not applied")
	await js("globalThis.probeObject={$godot:'Object',handle:probeHandle};globalThis.objectArgs={node:probeObject,items:[probeObject,null],counter:{$godot:'int64',value:'7'},wide:{$godot:'int64',value:'9223372036854775807'}};globalThis.objectResult=probeModule.call(probeTag,probeHandle,'objects',[objectArgs]);globalThis.asyncObject=null;probeModule.callAsync(probeTag,probeHandle,'objectsAsync',[objectArgs]).then(value=>{asyncObject=value;});undefined;")
	check(ReactNativeHost.get_last_error().is_empty(), "Object-typed script invocation failed: %s" % ReactNativeHost.get_last_error())
	var object_check := "%s.node.$godot==='Object' && %s.node.handle===probeHandle && %s.items[0].handle===probeHandle && %s.items[1]===null && %s.counter.$godot==='int64' && %s.counter.value==='7' && %s.wide.$godot==='int64' && %s.wide.value==='9223372036854775807'"
	check(await js(object_check.replace("%s", "objectResult")), "Nested Object result lost its wrappers or nullable element")
	check(await js("probeModule.call(probeTag,probeHandle,'nullable',[null])===null && probeModule.call(probeTag,probeHandle,'nullable',['value'])==='value'"), "Nullable Variant argument failed its declared contract")
	check(await js("probeModule.call(probeTag,probeHandle,'pair',[0.5])===2.5 && (()=>{try{probeModule.call(probeTag,probeHandle,'pair',[]);return false;}catch(e){return e.code==='E_VALIDATION';}})()"), "Trailing native default bypassed the required argument")
	for definition in [{"method": "typed_string", "value": {"type": "string"}}, {"method": "typed_integer", "value": {"type": "integer"}}, {"method": "typed_array", "value": {"type": "array", "element": {"type": "string"}}}]:
		for nested in [false, true]:
			var incompatible: RNSceneBinding = probe.binding()
			var nullable_argument := {"name": "value", "value": definition.value.duplicate(true)}
			if nested:
				nullable_argument.value.nullable = true
			else:
				nullable_argument.nullable = true
			incompatible.commands = {"invalid": {"method": definition.method, "mode": "sync", "arguments": [nullable_argument], "result": {"type": "void"}}}
			check(not probe_root.attach_scene_binding(probe, incompatible).is_empty(), "Nullable schema accepted a typed %s parameter" % definition.method)
	for schema_default in [false, true]:
		var invalid_order: RNSceneBinding = probe.binding()
		invalid_order.commands.pair.arguments[0].optional = true
		if schema_default:
			invalid_order.commands.pair.arguments[0].default = 1.0
		invalid_order.commands.pair.arguments[1].optional = false
		check(not probe_root.attach_scene_binding(probe, invalid_order).is_empty(), "Required argument accepted after an optional argument")
	var bad: RNSceneBinding = probe.binding()
	bad.snapshot_method = &"missing"
	check(not probe_root.attach_scene_binding(probe, bad).is_empty(), "Missing method accepted")
	bad = probe.binding()
	bad.commands.adjust.arguments[0].value.type = "string"
	check(not probe_root.attach_scene_binding(probe, bad).is_empty(), "Wrong signature accepted")
	bad = probe.binding()
	bad.signals.sampled.arguments = ["temperature"]
	check(not probe_root.attach_scene_binding(probe, bad).is_empty(), "Wrong signal arity accepted")
	probe.malformed = true
	check(await js("(()=>{try{probeModule.read(probeTag,probeHandle);return false;}catch(e){return e.code==='E_VALIDATION';}})()"), "Malformed return bypassed its schema")
	probe.malformed = false

	if not await wait_until(func(): return await js("asyncObject !== null && probeEvents.filter(event=>event.event==='reference').length===2"), "queued Object results and signals"):
		return
	var delivered: Array = await js("probeEvents")
	var samples := delivered.filter(func(event): return event.event == "sample")
	check(not samples.is_empty() and samples.back().payload.temperature == 19.0, "Signal argument mapping failed")
	check(await js(object_check.replace("%s", "asyncObject") + " && probeEvents.filter(event=>event.event==='reference').every(event=>event.payload.node.$godot==='Object' && event.payload.node.handle===probeHandle)"), "Queued Object results or signal payloads lost their scoped wrappers")
	check(delivered.back().sequence > delivered.front().sequence, "Scene sequence did not advance")

	var probe_tag := probe_root.get_root_tag()
	await js("probeModule.call(probeTag,probeHandle,'restart',[]);undefined;")
	if not await wait_until(func(): return probe_root.get_root_tag() != probe_tag, "a synchronous script call to restart its root"):
		return
	check(await js("(()=>{try{probeModule.read(probeTag,probeHandle);return false;}catch(e){return e.code==='E_STALE_HANDLE';}})()"), "A stopped root still served its handle")
	if not await wait_until(probe_ready, "the restarted root to read the binding"):
		return
	remove_child(probe)
	if not await wait_until(func(): return not await probe_ready(), "tree departure to clear readiness"):
		return
	add_child(probe)
	if not await wait_until(probe_ready, "re-entry to renew the binding"):
		return
	probe.queue_free()
	if not await wait_until(func(): return not await probe_ready(), "target death to clear readiness"):
		return
	probe_root.queue_free()
