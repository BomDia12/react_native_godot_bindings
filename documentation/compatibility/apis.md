# API compatibility

Public cross-platform non-component exports from React Native 0.87.1 are classified
below. Preamble shims prove fixture compatibility only. APIs that React Native scopes to a
single platform are out of scope and are not listed: `ActionSheetIOS`, `BackHandler`,
`DynamicColorIOS`, `PermissionsAndroid`, `PushNotificationIOS`, `Settings`, and
`ToastAndroid`. `InteractionManager` is absent from the tagged public types and its
development-only compatibility getter throws when accessed.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| API-ACCESSIBILITY | `AccessibilityInfo` | pending | No native service. | none | none |
| API-ALERT | `Alert` | pending | No native service. | none | none |
| API-ANIMATED | `Animated` | pending | Animation integration is not verified. | none | none |
| API-APPEARANCE | `Appearance` | pending | A fixed preamble shim is not production support. | none | none |
| API-APP-REGISTRY | `AppRegistry` | partially supported | Registers components and starts independent Fabric surfaces; Godot assigns process-wide root tags, gives each surface an operation queue with commit coalescing and imperative ordering, isolates surface failures, and does not implement the full native lifecycle contract. | [bundle entry](../../samples/view-text/godot.entry.js), [applications](../../samples/view-text/multi_root.entry.js), [runtime coordinator](../../modules/react_native_bindings/runtime/react_native_runtime_coordinator.cpp), [root view](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [MULTI-ROOT-SMOKE](test-coverage.md) |
| API-APP-STATE | `AppState` | pending | No native service. | none | none |
| API-ASSET-REGISTRY | `AssetRegistry` | partially supported | Metro assets are registered and resolved to validated `res://`/`user://` Godot URIs with scale selection; arbitrary native asset loading remains outside the contract. | [Godot asset plugin](../../js/godot/assets.cjs), [asset resolver](../../js/godot/adapters/resolveAssetSource.godot.js) | [GODOT-ASSETS-SMOKE](test-coverage.md), [JS-ADAPTER-UNIT](test-coverage.md) |
| API-CLIPBOARD | `Clipboard` | pending | No native service. | none | none |
| API-CODEGEN-COMMANDS | `codegenNativeCommands` | pending | Codegen pipeline is absent. | none | none |
| API-CODEGEN-COMPONENT | `codegenNativeComponent` | pending | Codegen pipeline is absent. | none | none |
| API-DEVICE-EVENT | `DeviceEventEmitter` | pending | Native event bridge is absent. | none | none |
| API-EVENT-EMITTER | `EventEmitter` | pending | The public JavaScript emitter is not covered by repository tests. | none | none |
| API-DEVICE-INFO | `DeviceInfo` | pending | Fixed fixture constants are not a native implementation. | none | none |
| API-DEV-MENU | `DevMenu` | pending | Developer service is absent. | none | none |
| API-DEV-SETTINGS | `DevSettings` | pending | Developer service is absent. | none | none |
| API-DIMENSIONS | `Dimensions` | pending | Fixed fixture constants do not track Godot display state. | none | none |
| API-EASING | `Easing` | pending | JS implementation is not verified. | none | none |
| API-FIND-NODE | `findNodeHandle` | partially supported | Returns the pinned numeric tag for tested host, composite, and saved unmounted refs and preserves numeric/null inputs; broader renderer identity semantics are not verified. | [root view registry](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [MULTI-ROOT-SMOKE](test-coverage.md) |
| API-I18N | `I18nManager` | pending | RTL and locale integration are absent. | none | none |
| API-KEYBOARD | `Keyboard` | pending | No native service. | none | none |
| API-LAYOUT-ANIMATION | `LayoutAnimation` | pending | Layout animation is absent. | none | none |
| API-LINKING | `Linking` | pending | No native service. | none | none |
| API-LOGBOX | `LogBox` | pending | Preamble no-op is not support. | none | none |
| API-NATIVE-APP-EVENT | `NativeAppEventEmitter` | pending | Native event bridge is absent. | none | none |
| API-COMPONENT-REGISTRY | `NativeComponentRegistry` | partially supported | Native descriptor lookup and view-config metadata are exposed to React Native before initialization; this is a Godot registry adaptation, not the complete platform registry contract. | [descriptor registry](../../modules/react_native_bindings/components/rn_host_descriptor_registry.cpp), [Godot bootstrap hooks](../../js/godot/bootstrap.js) | [DESCRIPTOR-REGISTRY-SMOKE](test-coverage.md) |
| API-NATIVE-EVENT | `NativeEventEmitter` | pending | Native event bridge is absent. | none | none |
| API-NATIVE-MODULES | `NativeModules` | pending | General native-module bridge is absent. | none | none |
| API-NETWORKING | `Networking` | pending | Networking implementation is absent. | none | none |
| API-PAN-RESPONDER | `PanResponder` | pending | PanResponder creation, responder negotiation, and gesture-state calculations are not verified; routed Godot input events do not establish the tagged PanResponder contract. | none | none |
| API-PIXEL-RATIO | `PixelRatio` | pending | Fixed fixture scale is not native support. | none | none |
| API-PLATFORM | `Platform` | partially supported | Godot is selected as `Platform.OS`; `select`, constants, version, and asset scale are adapted through the registered `PlatformConstants` module, while native platform fields and services are absent. | [Godot Platform adapter](../../js/godot/adapters/Platform.godot.js), [PlatformConstants module](../../modules/react_native_bindings/native_modules/rn_builtin_native_modules.cpp) | [GODOT-PLATFORM-SMOKE](test-coverage.md) |
| API-PLATFORM-COLOR | `PlatformColor` | pending | No Godot color contract is implemented. | none | none |
| API-PROCESS-COLOR | `processColor` | partially supported | Public string and numeric color inputs are normalized to validated Godot RGBA values; packed ARGB is not used on the native wire, and dynamic/platform colors remain unsupported. | [Godot color adapter](../../js/godot/adapters/processColor.godot.js), [color codec](../../js/godot/color.cjs), [view style](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | [STYLE-UNIT](test-coverage.md), [JS-ADAPTER-UNIT](test-coverage.md) |
| API-REGISTER-CALLABLE | `registerCallableModule` | pending | Preamble registry is fixture-only. | none | none |
| API-REQUIRE-NATIVE | `requireNativeComponent` | partially supported | Resolves registered Godot host descriptors and their view configs; unknown names fail before publication. This is a Godot descriptor adaptation, not Codegen or a complete native component library. | [descriptor registry](../../modules/react_native_bindings/components/rn_host_descriptor_registry.cpp), [Godot RN overrides](../../js/godot/rn-overrides.cjs) | [DESCRIPTOR-REGISTRY-SMOKE](test-coverage.md) |
| API-RN-VERSION | `ReactNativeVersion` | pending | JS export is not verified. | none | none |
| API-ROOT-TAG | `RootTagContext` | pending | JS export is not independently verified. | none | none |
| API-SHARE | `Share` | pending | No native service. | none | none |
| API-STYLESHEET | `StyleSheet` | partially supported | Flattened basic styles reach Fabric; the complete style contract is not supported. | [fixture](../../samples/view-text/godot.entry.js) | [BASELINE-SMOKE](test-coverage.md) |
| API-SYSTRACE | `Systrace` | pending | No tracing integration. | none | none |
| API-TURBO-REGISTRY | `TurboModuleRegistry` | partially supported | `get`/`getEnforcing` resolve schema-validated Godot native modules with sync, async, events, and cancellation; arbitrary platform TurboModules and generated Codegen modules remain absent. | [native-module registry](../../modules/react_native_bindings/native_modules/rn_native_module_registry.cpp), [bootstrap](../../js/godot/bootstrap.js) | [NATIVE-MODULE-SMOKE](test-coverage.md) |
| API-UI-MANAGER | `UIManager` | pending | Legacy Paper UIManager is absent. | none | none |
| API-BATCHED-UPDATES | `unstable_batchedUpdates` | partially supported | The public callback batches two state updates into one observed render in the fixture; broader scheduler and bookkeeping semantics are not verified. | [multi-root entry](../../samples/view-text/multi_root.entry.js) | [MULTI-ROOT-SMOKE](test-coverage.md) |
| API-ANIMATED-VALUE | `useAnimatedValue` | pending | Animation integration is not verified. | none | none |
| API-ANIMATED-VALUE-XY | `useAnimatedValueXY` | pending | Animation integration is not verified. | none | none |
| API-ANIMATED-COLOR | `useAnimatedColor` | pending | Animation integration is not verified. | none | none |
| API-COLOR-SCHEME | `useColorScheme` | pending | Appearance service is absent. | none | none |
| API-PRESSABILITY | `usePressability` | partially supported | Returns handlers that work through routed mouse, touch, keyboard, focus, hover, and responder events on retained View hosts; long-press and press-retention edge cases, production feature flags, and broader platform coverage remain unverified. | [input router](../../modules/react_native_bindings/input/rn_input_router.cpp), [event bridge](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp), [mounting manager](../../modules/react_native_bindings/mounting/rn_mounting_manager.cpp) | [PRESSABLE-SMOKE](test-coverage.md) |
| API-WINDOW-DIMENSIONS | `useWindowDimensions` | pending | Native dimensions service is absent. | none | none |
| API-UTF-SEQUENCE | `UTFSequence` | pending | JS export is not verified. | none | none |
| API-VIBRATION | `Vibration` | pending | No native service. | none | none |
| API-VIRTUAL-ARRAY | `unstable_VirtualArray` | pending | Experimental virtual-collection behavior is not verified. | none | none |
| API-VIRTUAL-COLLECTION | `unstable_createVirtualCollectionView` | pending | Experimental virtual-collection behavior is not verified. | none | none |
| API-VIRTUAL-COLUMN-GENERATOR | `unstable_VirtualColumnGenerator` | pending | Experimental virtual-collection behavior is not verified. | none | none |
| API-SCROLL-PARENT | `unstable_getScrollParent` | pending | Experimental DOM and scrolling behavior is not verified. | none | none |
| API-VIRTUAL-INITIAL-COUNT | `unstable_DEFAULT_INITIAL_NUM_TO_RENDER` | pending | Experimental virtual-collection behavior is not verified. | none | none |
| API-VIRTUAL-MODE | `VirtualViewMode` | pending | Experimental virtual-view behavior is not verified. | none | none |
