# C++ extension contracts

Phase 5 extensions are registered in C++ before the first bundle starts. Definitions
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
