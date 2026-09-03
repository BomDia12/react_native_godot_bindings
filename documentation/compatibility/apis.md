# API compatibility

Public cross-platform non-component exports from React Native 0.87.1 are classified
below. Preamble shims prove fixture compatibility only. APIs that React Native scopes to a
single platform are out of scope and are not listed: `ActionSheetIOS`, `BackHandler`,
`DynamicColorIOS`, `NativeDialogManagerAndroid`, `PermissionsAndroid`,
`PushNotificationIOS`, `Settings`, and `ToastAndroid`.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| API-ACCESSIBILITY | `AccessibilityInfo` | pending | No native service. | none | none |
| API-ALERT | `Alert` | pending | No native service. | none | none |
| API-ANIMATED | `Animated` | pending | Animation integration is not verified. | none | none |
| API-APPEARANCE | `Appearance` | pending | A fixed preamble shim is not production support. | none | none |
| API-APP-REGISTRY | `AppRegistry` | partially supported | The fixture registers and starts one Fabric application surface. | [entry](../../samples/view-text/godot.entry.js) | [BASELINE-SMOKE](test-coverage.md) |
| API-APP-STATE | `AppState` | pending | No native service. | none | none |
| API-CLIPBOARD | `Clipboard` | pending | No native service. | none | none |
| API-CODEGEN-COMMANDS | `codegenNativeCommands` | pending | Codegen pipeline is absent. | none | none |
| API-CODEGEN-COMPONENT | `codegenNativeComponent` | pending | Codegen pipeline is absent. | none | none |
| API-DEVICE-EVENT | `DeviceEventEmitter` | pending | Native event bridge is absent. | none | none |
| API-EVENT-EMITTER | `EventEmitter` | pending | The public emitter has no Godot event-source adaptation. | none | none |
| API-DEVICE-INFO | `DeviceInfo` | pending | Fixed fixture constants are not a native implementation. | none | none |
| API-DEV-MENU | `DevMenu` | pending | Developer service is absent. | none | none |
| API-DEV-SETTINGS | `DevSettings` | pending | Developer service is absent. | none | none |
| API-DIMENSIONS | `Dimensions` | pending | Fixed fixture constants do not track Godot display state. | none | none |
| API-EASING | `Easing` | pending | JS implementation is not verified. | none | none |
| API-FIND-NODE | `findNodeHandle` | pending | Public instance and ref contract are absent. | none | none |
| API-I18N | `I18nManager` | pending | RTL and locale integration are absent. | none | none |
| API-INTERACTION | `InteractionManager` | pending | Scheduling contract is not verified. | none | none |
| API-KEYBOARD | `Keyboard` | pending | No native service. | none | none |
| API-LAYOUT-ANIMATION | `LayoutAnimation` | pending | Layout animation is absent. | none | none |
| API-LINKING | `Linking` | pending | No native service. | none | none |
| API-LOGBOX | `LogBox` | pending | Preamble no-op is not support. | none | none |
| API-NATIVE-APP-EVENT | `NativeAppEventEmitter` | pending | Native event bridge is absent. | none | none |
| API-COMPONENT-REGISTRY | `NativeComponentRegistry` | pending | No Godot component descriptor registry is implemented. | none | none |
| API-NATIVE-EVENT | `NativeEventEmitter` | pending | Native event bridge is absent. | none | none |
| API-NATIVE-MODULES | `NativeModules` | pending | General native-module bridge is absent. | none | none |
| API-NETWORKING | `Networking` | pending | Networking implementation is absent. | none | none |
| API-PAN-RESPONDER | `PanResponder` | pending | Input and responder path are absent. | none | none |
| API-PIXEL-RATIO | `PixelRatio` | pending | Fixed fixture scale is not native support. | none | none |
| API-PLATFORM | `Platform` | pending | Supported bundle currently reports Android, not Godot. | none | none |
| API-PLATFORM-COLOR | `PlatformColor` | pending | No Godot color contract is implemented. | none | none |
| API-PROCESS-COLOR | `processColor` | partially supported | Android signed-ARGB output is decoded by the renderer; Godot-native and dynamic colors are absent. | [color conversion](../../modules/react_native_bindings/fabric/rn_view_style.cpp) | [STYLE-UNIT](test-coverage.md) |
| API-REGISTER-CALLABLE | `registerCallableModule` | pending | Preamble registry is fixture-only. | none | none |
| API-REQUIRE-NATIVE | `requireNativeComponent` | pending | Host-component registry contract is absent. | none | none |
| API-RN-VERSION | `ReactNativeVersion` | pending | JS export is not verified. | none | none |
| API-ROOT-TAG | `RootTagContext` | pending | JS export is not independently verified. | none | none |
| API-SHARE | `Share` | pending | No native service. | none | none |
| API-STYLESHEET | `StyleSheet` | partially supported | Flattened basic styles reach Fabric; the complete style contract is not supported. | [fixture](../../samples/view-text/godot.entry.js) | [BASELINE-SMOKE](test-coverage.md) |
| API-SYSTRACE | `Systrace` | pending | No tracing integration. | none | none |
| API-TURBO-REGISTRY | `TurboModuleRegistry` | pending | Preamble shims are not a general registry. | none | none |
| API-UI-MANAGER | `UIManager` | pending | Legacy Paper UIManager is absent. | none | none |
| API-BATCHED-UPDATES | `unstable_batchedUpdates` | pending | Renderer behavior is not verified. | none | none |
| API-ANIMATED-VALUE | `useAnimatedValue` | pending | Animation integration is not verified. | none | none |
| API-ANIMATED-VALUE-XY | `useAnimatedValueXY` | pending | Animation integration is not verified. | none | none |
| API-ANIMATED-COLOR | `useAnimatedColor` | pending | Animation integration is not verified. | none | none |
| API-COLOR-SCHEME | `useColorScheme` | pending | Appearance service is absent. | none | none |
| API-PRESSABILITY | `usePressability` | partially supported | Works through the routed mouse, touch, keyboard, focus, hover, and responder subset; durable native identity, complete measurement, pointer capture, long-press edge cases, production feature flags, and broader platform coverage are absent. | [input router](../../modules/react_native_bindings/input/rn_input_router.cpp), [event bridge](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [PRESSABLE-SMOKE](test-coverage.md) |
| API-WINDOW-DIMENSIONS | `useWindowDimensions` | pending | Native dimensions service is absent. | none | none |
| API-UTF-SEQUENCE | `UTFSequence` | pending | JS export is not verified. | none | none |
| API-VIBRATION | `Vibration` | pending | No native service. | none | none |
