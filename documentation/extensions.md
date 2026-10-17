# C++ extension contracts

Extensions follow React Native's own pattern: a codegen spec in JavaScript, plus a C++
component view or TurboModule that implements it. Godot specs live in
`packages/react-native-godot/specs/`. `node packages/react-native-godot/scripts/codegen.cjs`
regenerates `modules/react_native_bindings/generated/`, which is committed. C++ sources
belong in `components/`, `turbomodules/` or `examples/`; each source directory must appear
in `module_source_dirs` in `modules/react_native_bindings/SCsub`.

## Component views

A native component starts as a codegen spec. The example meter's spec is
`specs/RNExampleMeterNativeComponent.js`:

```js
type NativeProps = $ReadOnly<{
  ...ViewProps,
  value?: Double,
  tint?: ?ColorValue,
  onValueChanged?: ?DirectEventHandler<ValueChangedEvent>,
}>;

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['advance'],
});

export default (codegenNativeComponent<NativeProps>('RNExampleMeter'): NativeType);
```

Codegen produces the props, event emitter, shadow node and component descriptor. A
component that needs its own measurement or state passes `{interfaceOnly: true}` and
supplies a shadow node under `react_platform/`, as `GodotWindow`, `GodotTextInput` and
`Switch` do.

On the Godot side a component view owns one Control and translates upstream props,
state, layout and events into it on the main thread:

```cpp
void RNExampleMeterComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const RNExampleMeterProps &>(*p_new);
	meter->set_value_no_signal(next.value);
}

void RNExampleMeterComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const RNExampleMeterEventEmitter>(p_emitter);
	value_changed = emitter ? std::function<void(double)>([emitter](double p_value) { emitter->onValueChanged({ .value = p_value }); }) : nullptr;
}
```

`RNComponentView` places children by counting only React-owned siblings, so Godot
children authored in a scene survive every mutation. Override `get_child_container()`
when children belong inside an internal control, `handle_command()` for commands,
`get_input_controls()` and `owns_input_control()` when internal controls receive input,
`get_focus_control()` when another control takes focus, and `owns_activation()` when the
Godot control handles presses itself. Controls return an empty combined minimum size and
an unbounded combined maximum size, so React Native's layout sizes them exactly.

Register the descriptor and the view together in `host/react_native_host.cpp`: add
`concreteComponentDescriptorProvider<RNExampleMeterComponentDescriptor>()` to
`component_registry_factory()` and the view factory to `component_view_factories()`. A
name without a view factory reports `E_UNKNOWN_COMPONENT` and mounts a plain View.

JavaScript uses the spec directly:

```js
import RNExampleMeter, {Commands} from 'react-native-godot/specs/RNExampleMeterNativeComponent';

<RNExampleMeter ref={meter} style={{width: 80, height: 20}} value={0.25}
  onValueChanged={event => setValue(event.nativeEvent.value)} />;
Commands.advance(meter.current, 0.25);
```

## TurboModules

A module spec declares its methods and events; `specs/NativeExampleScene.js` is the
example:

```js
export interface Spec extends TurboModule {
  +getConstants: () => {name: string, version: number};
  +read: (rootTag: RootTag) => CounterValue;
  +incrementLater: (rootTag: RootTag, amount: number) => Promise<CounterValue>;
  +onChanged: EventEmitter<CounterValue>;
}
```

The C++ class derives from the generated `NativeExampleSceneCxxSpec` and receives an
`RNHostModuleContext` with the main-thread queue and a root lookup. Methods run on the JS
thread. A synchronous method that needs Godot uses `main_thread->run_sync`, which blocks
the JS thread until the main thread runs the work; an asynchronous method returns an
`RNJSPromise` and settles it from the main thread. Never touch nodes from the JS thread,
and never make the main thread wait for JavaScript. Events emit through the generated
`emitOn...` methods or the module's event emitter from any thread. Register the factory in
`RNTurboModuleContext::extensions` where `host/react_native_host.cpp` builds the
TurboModule context. Specs whose values are only known at runtime use names ending in
`Cxx`, so codegen passes `mixed` values through as `jsi::Value`.

`RNImageTransport` connects the image service to a transport. Start, completion and cancel run on the main thread.
Transport must enforce the encoded ceiling before growing response buffers, honor
headers and include/omit credentials, and mark no-store/no-cache responses
uncacheable. `cancel` returns true only when transport allocations are released;
otherwise completion must still occur so the held reservation can be released.
Credentialed requests, including explicit Authorization/Cookie, bypass shared cache
and deduplication. Only explicitly credential-free requests share results. Image
limits under `react_native/images/` are read when the host starts; cache
zero disables cache ownership, while other ceilings must be positive. Mounted
textures retain their budget reservation after cache eviction. Static PNG/JPEG/WebP
codecs and imported native textures are bounded before decoding or allocation.

## Script-authored scene capabilities

The generic GodotScene module carries application schemas at runtime. New scripts and
resources attach an RNSceneBinding without recompiling C++:

```gdscript
var resource := RNSceneBinding.new()
resource.capability = "ClimateSensor"
resource.snapshot_method = &"snapshot"
resource.snapshot_schema = {"type": "record", "fields": {"temperature": {"type": "number"}}}
resource.commands = {"adjust": {"method": "adjust", "mode": "sync",
    "arguments": [{"name": "amount", "value": {"type": "number"}}],
    "result": {"type": "number"}}}
var error := root.attach_scene_binding(sensor, resource)
```

Schema versioning belongs to the project: change `schema_version` with incompatible
payloads. [Interop](interop.md#script-capability-resources) describes validation and
readiness/sequence ownership. The lifecycle smoke also loads an equivalent `.tres`.

## Root registration and Alerts

```js
import {GodotAppRegistry, useGodotAlert} from 'react-native-godot/app-registry';
GodotAppRegistry.registerComponent('Inventory', () => Inventory, {alerts: true});
// Inside Inventory: const alert = useGodotAlert(); await alert('Saved', 'Done');
```

Options are Godot helper options; upstream AppRegistry's third argument remains the
`section` boolean (`options.section` forwards it). `alerts` defaults to false, which
bubbles root alerts to Godot handlers; `true` presents a Godot AcceptDialog in the root.
A custom function receives the presentation payload and returns or resolves
`{buttonId, dismissed}`, or `{handled: false}` to decline. `useGodotRoot()` exposes the
root tag and its alert function. Plain `Alert.alert` is not tied to a root and goes to
the application handler.

A root can set `set_alert_handler(callable)`; the application handler is installed with
`GodotAlerts.set_alert_handler(callable)`. The Callable receives
`(requestId, origin, payload)`, where `origin` is `{rootTag}` or null, and completes
asynchronously with `root.complete_alert(requestId, {buttonId: 0, dismissed: false})` or
`GodotAlerts.complete_alert(requestId, result)`. Its return value is ignored. Without any
handler, a native dialog presents the alert. A decline bubbles once; a stopped root
cancels its pending alerts. Noncancelable native alerts reopen after a window-close
attempt. Results have a numeric `buttonId` and `dismissed: false`, or `buttonId: null`
and `dismissed: true`.
