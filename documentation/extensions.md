# C++ extension contracts

Extensions are registered in C++ before the first bundle starts. Definitions
contain no JSI values and are frozen at scene initialization. Implementations belong in
`components/`, `native_modules/`, or `examples/`; each source directory must appear in
`module_source_dirs` in `modules/react_native_bindings/SCsub`.

## Host descriptors

A descriptor owns host creation, pure prop preparation, complete application, state
capture/restore, measurement, signal wiring, commands, events, and traits. The working
example registers through the same API as View and Text:

```cpp
bool rn_register_example_meter(RNHostDescriptorRegistry &p_registry,
        RNError &r_error) {
    return p_registry.register_descriptor(
            std::make_shared<RNExampleMeterDescriptor>(), r_error);
}
```

`RNExampleMeterDescriptor` derives from `RNHostDescriptor`, is named
`RNExampleMeter`, creates a `ProgressBar`, validates `value` and `tint` in `prepare()`,
measures to an intrinsic 80×20, and implements `advance`. Its signal wiring is:

```cpp
void attach_signals(Control *p_host, const RNHostContext &p_context) const override {
    const Callable callback = callable_mp(
            p_context.owner,
            &ReactNativeRootView::_on_descriptor_value_changed)
            .bind(p_context.tag, p_host->get_instance_id());
    if (!p_host->is_connected("value_changed", callback)) {
        p_host->connect("value_changed", callback);
    }
}

void detach_signals(Control *p_host, const RNHostContext &p_context) const override {
    const Callable callback = callable_mp(
            p_context.owner,
            &ReactNativeRootView::_on_descriptor_value_changed)
            .bind(p_context.tag, p_host->get_instance_id());
    if (p_host->is_connected("value_changed", callback)) {
        p_host->disconnect("value_changed", callback);
    }
}
```

The matching JavaScript consumer uses normal React Native APIs:

```js
const RNExampleMeter = requireNativeComponent('RNExampleMeter');

<RNExampleMeter
  ref={meter}
  value={0.25}
  tint="#33cc66"
  onValueChanged={event => setValue(event.nativeEvent.value)}
/>

UIManager.dispatchViewManagerCommand(
  findNodeHandle(meter.current),
  'advance',
  [0.25],
);
```

Preparation must finish before scene mutation. Every apply path must describe the whole
owned state. Capture state before updates and restore it if any later mutation fails;
connect signals only for published hosts and disconnect them on removal. Unknown names
fail with `E_UNKNOWN_COMPONENT` and retain the previous published tree. Adding a new
component-name switch to the root view or mounting manager bypasses this contract and is
not supported.

## Native modules

Schemas are handwritten from `RNValueSchema`, `RNMethodSchema`, and `RNEventSchema`.
This is the complete registration shape used by the example module (the helper functions
construct the record schemas shown in `examples/rn_example_scene_module.cpp`):

```cpp
RNModuleDefinition definition;
definition.name = "ExampleScene";

RNMethodSchema echo = method("echo", echo_schema());
echo.arguments.push_back(argument("value", echo_schema()));
definition.methods.push_back(echo);

RNMethodSchema increment = method(
        "incrementLater", counter_value_schema(), RNCallMode::ASYNC, true);
increment.arguments.push_back(argument(
        "session", RNValueSchema::value(RNValueType::SESSION)));
RNValueSchema target = RNValueSchema::value(RNValueType::OBJECT);
target.capability = "ExampleCounter";
increment.arguments.push_back(argument("target", target));
increment.arguments.push_back(argument(
        "amount", RNValueSchema::value(RNValueType::FLOAT)));
definition.methods.push_back(increment);

RNEventSchema changed;
changed.name = "changed";
changed.subscription_name = "onChanged";
changed.payload = counter_value_schema();
changed.requires_session = true;
definition.events.push_back(changed);
definition.factory = []() { return std::make_unique<RNExampleSceneModule>(); };
return p_registry.register_module(definition, r_error);
```

The subscription member is exactly `onChanged`; names are not derived by capitalization.
Its JavaScript ownership and removal are explicit:

```js
const scene = TurboModuleRegistry.getEnforcing('ExampleScene');
const session = openSession(rootTag);
const target = scene.getTarget(session);
const subscription = scene.onChanged(session, event => setValue(event.value));

await callAsync(scene, 'incrementLater', [session, target, 2], {signal});

subscription.remove();
closeSession(session);
```

`global.__godotNativeModules.getSchema(name)` and a module proxy's `__godotSchema`
return copied schema metadata; changing the copy cannot alter the frozen native
definition. Callbacks are retained only in the generation
registry; native jobs carry tokens and copied Variants, never JSI references. The module
emits through `RNCompletionToken`, which queues delivery instead of entering Hermes.

## Initialization order

At `MODULE_INITIALIZATION_LEVEL_SCENE`, `register_types.cpp` registers built-in and
project descriptors, freezes that registry, registers built-in and project modules,
freezes module definitions, and then exposes scene classes. Registration rejects empty,
duplicate, invalid, or reserved names before a bundle evaluates. Core initialization
creates the runtime/coordinator only and performs no scene-dependent registration.

## Stateful host publication

Resource resolution runs on the main thread before Yoga and produces immutable
component data and a dependency revision. Measured leaves and independent scroll
or presentation child layouts use descriptor policies. Internal controls are owned
by the descriptor's child container; renderer ordering touches only React children.
The external-layout native opt-in lets Yoga constrain controls without changing
ordinary Godot minimum-size behavior.

Capture native state before mutation. Editor checkpoints include native undo/redo,
preedit and pending notification state; string setters are not a rollback mechanism.
`after_publish` runs parent first only after publication and owns asynchronous work,
subscriptions and presentation activation. The copied event sink checks generation,
surface epoch, tag, native object identity and revision. Completion code must also
check `is_current` before applying native state. Visual snapshots use actual native
transforms, viewport clips, scroll positions and TextServer span bounds.

`RNImageTransport` is the 6B handoff. Start/completion/cancel run on the main thread.
Transport must enforce the encoded ceiling before growing response buffers, honor
headers and include/omit credentials, and mark no-store/no-cache responses
uncacheable. `cancel` returns true only when transport allocations are released;
otherwise completion must still occur so the held reservation can be released.
Credentialed requests, including explicit Authorization/Cookie, bypass shared cache
and deduplication. Only explicitly credential-free requests share results. Image
limits under `react_native/images/` are snapshotted per runtime generation; cache
zero disables cache ownership, while other ceilings must be positive. Mounted
textures retain their budget reservation after cache eviction. Static PNG/JPEG/WebP
codecs and imported native textures are bounded before decoding or allocation.

## Script-authored scene capabilities

The frozen native registry contains the generic GodotScene module, not application
schemas. New scripts/resources attach an RNSceneBinding without recompiling C++:

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
`section` boolean (`options.section` forwards it). `alerts` defaults to false; true
uses a local Godot AcceptDialog. A custom function receives the copied presentation
payload and returns/resolves `{buttonId, dismissed}` or `{handled:false}` to decline.
The request stays bound to its originating root/epoch. `useGodotRoot()` exposes that
session/origin and root-bound alert function; store the function for timers/promises.
No ambient scope survives a microtask automatically.

A root can set `set_alert_handler(callable)`; the application fallback is installed with `GodotAlerts.set_alert_handler(callable)`.
The Callable receives `(requestId, origin, payload)` and completes asynchronously
with `root.complete_alert(requestId, {buttonId: 0, dismissed: false})`; the application
singleton `GodotAlerts.complete_alert(requestId, result)` completes unattributed requests.
Handlers complete explicitly; their return value is ignored.
Default originless requests remain originless. Decline bubbles once; presenter failure,
origin destruction/reload and stale stored callbacks cancel rather than selecting a
sibling. Native dialogs attach to the origin's presentation Window; Window's React
content retains the same root context.

## Native service lifecycle

Modules can implement `shutdown`, `on_session_closed`,
`on_surface_closed`, `on_scene_binding_changed`, `process_frame`, `has_pending_work`
and `on_result_delivered`. The registry owns generation instances and invokes shutdown
before JSI caches disappear. Copied native events are bounded and immutable; listener
snapshots retain a cursor when the frame budget is exhausted. Hermes checkpoints follow
each delivered callback. Native jobs never carry JSI references.

The shared image service receives the pooled HTTP adapter before bundle evaluation.
Its start/complete/cancel contract and existing descriptor publication hooks remain as
above; failed preparation starts no transport work. A module completion must honor the
generation and release retained response ownership when delivery/conversion fails.

`RNCompletionToken::emit` returns whether the bounded event queue accepted a
notification. Producers must retain a bounded retry or fail/remove their pending work
when it returns false. Custom Alert presentation rejects with `E_LIMIT` and removes its
request when the presentation notification cannot be queued. Native noncancelable
alerts reopen after a window-close attempt and remain available for a button choice.
