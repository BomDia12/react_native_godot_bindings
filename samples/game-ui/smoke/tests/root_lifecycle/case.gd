extends "res://smoke/game_test.gd"

var probe: Node
var probe_root: ReactNativeRootView
var initial_counts: Dictionary
var old_tag := 0
var sibling_tag := 0
var sibling_control := 0
func _process(_delta: float) -> void:
	if not advance(): return
	match stage:
		0:
			initial_counts = HermesRuntime.evaluate("__godotNativeModules.getStats()")
			old_tag = panels[0].get_root_tag()
			sibling_tag = panels[1].get_root_tag()
			sibling_control = panels[1].get_child(0).get_instance_id()
			enemies[0].apply_damage(13)
			panels[0].reload()
			next_stage()
		1:
			check(HermesRuntime.evaluate("__godotNativeModules.getStats()") == initial_counts, "Root reload leaked bridge resources: %s -> %s" % [initial_counts, HermesRuntime.evaluate("__godotNativeModules.getStats()")])
			check(panels[0].get_root_tag() != old_tag, "Reload reused a stale root session")
			check(fixture().enemies[str(panels[0].get_root_tag())].health == 87, "Root reload reset Godot gameplay")
			check(panels[1].get_root_tag() == sibling_tag and panels[1].get_child(0).get_instance_id() == sibling_control, "Sibling identity changed during reload")
			ui.remove_child(panels[0])
			next_stage()
		2:
			check(fixture().enemies.size() == 2, "Root departure retained React subscription")
			ui.add_child(panels[0])
			next_stage()
		3:
			check(fixture().enemies[str(panels[0].get_root_tag())].health == 87, "Re-entry lost persistent state")
			ReactNativeFileSingleton.force_refresh()
			next_stage()
		4:
			check(fixture().enemies.size() == 3 and fixture().enemies[str(panels[0].get_root_tag())].health == 87, "Bundle reset did not rebuild persistent Godot state")
			enemies[2].apply_damage(100)
			next_stage()
		5:
			check(fixture().enemies.size() == 2 and enemies[2].health == 0, "Dead enemy retained its surface")
			probe = preload("res://smoke/tests/root_lifecycle/probe.gd").new()
			probe_root = ReactNativeRootView.new()
			probe_root.application_key = "SceneProbe"
			probe_root.size = Vector2(200, 40)
			probe.ui = probe_root
			var authored: RNSceneBinding = probe.binding()
			var loaded: RNSceneBinding = load("res://smoke/tests/root_lifecycle/probe.tres")
			check(authored.snapshot_schema == loaded.snapshot_schema and authored.commands == loaded.commands and authored.signals == loaded.signals, "GDScript and resource schemas differ")
			check(probe_root.attach_scene_binding(probe, loaded).is_empty(), "Unrelated resource schema failed to attach")
			ui.add_child(probe_root)
			next_stage()
		6:
			check(not HermesRuntime.get_global("__probe").ready, "Target outside the tree became ready")
			HermesRuntime.evaluate("globalThis.probeModule=__godotNativeModules.get('GodotScene');globalThis.probeSession=__godotNativeModules.openSession(%d);globalThis.probeEvents=[];globalThis.probeSub=probeModule.onChanged(probeSession,e=>probeEvents.push(e));probeModule.getBinding(probeSession);undefined;" % probe_root.get_root_tag())
			add_child(probe)
			next_stage()
		7:
			check(HermesRuntime.get_global("__probe").ready, "Tree entry did not publish readiness")
			check(HermesRuntime.evaluate("probeEvents.some(e=>e.ready)"), "Subscribe-before-read missed readiness")
			HermesRuntime.evaluate("globalThis.probeHandle=probeModule.getBinding(probeSession).binding;globalThis.probeOther=__godotNativeModules.openSession(%d);globalThis.probeResults={};try{probeModule.read(probeOther,probeHandle);}catch(e){probeResults.cross=e.code;}probeResults.adjust=probeModule.call(probeSession,probeHandle,'adjust',[]);undefined;" % probe_root.get_root_tag())
			check(HermesRuntime.get_global("probeResults").cross == "E_STALE_HANDLE", "Handle crossed sessions")
			check(probe.temperature == 19.0, "Script optional default was not applied")
			HermesRuntime.evaluate("globalThis.probeObject={$godot:'Object',handle:probeHandle};globalThis.objectArgs={node:probeObject,items:[probeObject,null],counter:{$godot:'int64',value:'7'},wide:{$godot:'int64',value:'9223372036854775807'}};globalThis.objectResult=probeModule.call(probeSession,probeHandle,'objects',[objectArgs]);globalThis.asyncObject=null;probeModule.callAsync(probeSession,probeHandle,'objectsAsync',[objectArgs]).then(value=>{asyncObject=value;});undefined;")
			check(HermesRuntime.get_last_error().is_empty(), "Object-typed script invocation failed")
			check(HermesRuntime.evaluate("objectResult.node.$godot==='Object' && objectResult.node.handle===probeHandle && objectResult.items[0].handle===probeHandle && objectResult.items[1]===null && objectResult.counter.$godot==='int64' && objectResult.counter.value==='7' && objectResult.wide.$godot==='int64' && objectResult.wide.value==='9223372036854775807'"), "Nested Object result lost its wrappers or nullable element")
			check(HermesRuntime.evaluate("(()=>{try{probeModule.call(probeOther,probeModule.getBinding(probeOther).binding,'objects',[objectArgs]);return false;}catch(e){return e.code==='E_STALE_HANDLE';}})()"), "Nested Object argument crossed sessions")
			check(HermesRuntime.evaluate("probeModule.call(probeSession,probeHandle,'nullable',[null])===null && probeModule.call(probeSession,probeHandle,'nullable',['value'])==='value'"), "Nullable Variant argument failed its declared contract")
			check(HermesRuntime.evaluate("probeModule.call(probeSession,probeHandle,'pair',[0.5])===2.5 && (()=>{try{probeModule.call(probeSession,probeHandle,'pair',[]);return false;}catch(e){return e.code==='E_VALIDATION';}})()"), "Trailing native default bypassed the required argument")
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
			check(HermesRuntime.evaluate("(()=>{try{probeModule.read(probeSession,probeHandle);return false;}catch(e){return e.code==='E_VALIDATION';}})()"), "Malformed return bypassed its schema")
			probe.malformed = false
			next_stage()
		8:
			var delivered: Array = HermesRuntime.get_global("probeEvents")
			var samples := delivered.filter(func(event): return event.event == "sample")
			check(not samples.is_empty() and samples.back().payload.temperature == 19.0, "Signal argument mapping failed")
			check(HermesRuntime.evaluate("asyncObject && asyncObject.node.$godot==='Object' && asyncObject.items[0].handle===probeHandle && asyncObject.items[1]===null && asyncObject.counter.$godot==='int64' && asyncObject.counter.value==='7' && asyncObject.wide.$godot==='int64' && asyncObject.wide.value==='9223372036854775807' && probeEvents.filter(event=>event.event==='reference').length===2 && probeEvents.filter(event=>event.event==='reference').every(event=>event.payload.node.$godot==='Object' && event.payload.node.handle===probeHandle)"), "Queued Object results or signal payloads lost their scoped wrappers")
			check(delivered.back().sequence > delivered.front().sequence, "Scene sequence did not advance")
			old_tag = probe_root.get_root_tag()
			HermesRuntime.evaluate("probeModule.call(probeSession,probeHandle,'restart',[]);undefined;")
			next_stage()
		9:
			check(probe_root.get_root_tag() != old_tag, "Sync script root reload stalled Hermes")
			check(HermesRuntime.evaluate("(()=>{try{probeModule.read(probeSession,probeHandle);return false;}catch(e){return e.code==='E_SESSION_CLOSED';}})()"), "Stale session survived reload")
			remove_child(probe)
			next_stage()
		10:
			check(not HermesRuntime.get_global("__probe").ready, "Tree departure retained binding readiness")
			add_child(probe)
			next_stage()
		11:
			check(HermesRuntime.get_global("__probe").ready, "Re-entry did not renew binding")
			probe.queue_free()
			next_stage()
		12:
			check(not HermesRuntime.get_global("__probe").ready, "Target death retained opaque handles")
			probe_root.queue_free()
			next_stage()
		13:
			finish("game-root-lifecycle")
