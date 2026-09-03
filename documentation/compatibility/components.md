# Component compatibility

Public cross-platform component exports from React Native 0.87.1 are classified below.
`RCTRawText` is an internal host primitive folded into `Text`, not a public component
export. Components that React Native scopes to a single platform are out of scope and are
not listed: `DrawerLayoutAndroid`, `InputAccessoryView`, `ProgressBarAndroid`,
`SafeAreaView`, and `TouchableNativeFeedback`. `Touchable` remains a runtime compatibility
re-export but is absent from the tagged public types; `unstable_VirtualView` resolves to a
private source module and is excluded from this public matrix.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| COMP-ACTIVITY-INDICATOR | `ActivityIndicator` | pending | No native primitive. | none | none |
| COMP-BUTTON | `Button` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-FLAT-LIST | `FlatList` | pending | Requires scrolling and durable identity. | none | none |
| COMP-IMAGE | `Image` | pending | No native image primitive. | none | none |
| COMP-IMAGE-BACKGROUND | `ImageBackground` | pending | Depends on `Image`. | none | none |
| COMP-KEYBOARD-AVOIDING | `KeyboardAvoidingView` | pending | Keyboard and layout integration are absent. | none | none |
| COMP-LAYOUT-CONFORMANCE | `experimental_LayoutConformance` | pending | Experimental export is not verified. | none | none |
| COMP-MODAL | `Modal` | pending | No native modal primitive. | none | none |
| COMP-NATIVE-TEXT | `unstable_NativeText` | pending | Underlying host primitive exists but this public export is not verified. | none | none |
| COMP-NATIVE-VIEW | `unstable_NativeView` | pending | Underlying host primitive exists but this public export is not verified. | none | none |
| COMP-PRESSABLE | `Pressable` | partially supported | Mouse, touch, keyboard, focus, hover, press, responder, capture, and bubble paths work through Godot input routing; durable native identity, complete measurement, pointer capture, long-press edge cases, production feature flags, and broader platform coverage are absent. | [input router](../../modules/react_native_bindings/input/rn_input_router.cpp), [event bridge](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp) | [PRESSABLE-SMOKE](test-coverage.md) |
| COMP-REFRESH-CONTROL | `RefreshControl` | pending | Scrolling and refresh behavior are absent. | none | none |
| COMP-SCROLL-VIEW | `ScrollView` | pending | No scrolling primitive or retained position. | none | none |
| COMP-SECTION-LIST | `SectionList` | pending | Depends on `ScrollView` and durable identity. | none | none |
| COMP-STATUS-BAR | `StatusBar` | pending | No Godot adaptation is defined. | none | none |
| COMP-SWITCH | `Switch` | pending | No native switch primitive. | none | none |
| COMP-TEXT | `Text` | partially supported | Mounts one Godot `Label`; nested spans, selection, bidi, and full typography are absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| COMP-TEXT-ANCESTOR | `unstable_TextAncestorContext` | pending | Public JS context export is not verified. | none | none |
| COMP-TEXT-INPUT | `TextInput` | pending | Requires durable identity, focus, selection, and IME support. | none | none |
| COMP-TOUCHABLE-HIGHLIGHT | `TouchableHighlight` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-TOUCHABLE-OPACITY | `TouchableOpacity` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-TOUCHABLE-WITHOUT | `TouchableWithoutFeedback` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-VIEW | `View` | partially supported | Mounts a Godot `Panel` with Yoga layout, visual styles, pointer hit testing, layout events, and routed mouse, touch, keyboard, focus, and hover events; accessibility, transforms, and persistent identity are absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp), [input router](../../modules/react_native_bindings/input/rn_input_router.cpp) | [BASELINE-SMOKE](test-coverage.md), [INTERACTION-UNIT](test-coverage.md), [PRESSABLE-SMOKE](test-coverage.md) |
| COMP-VIRTUALIZED-LIST | `VirtualizedList` | pending | Requires scrolling, measurement, and durable identity. | none | none |
| COMP-VIRTUALIZED-SECTION | `VirtualizedSectionList` | pending | Requires scrolling, measurement, and durable identity. | none | none |
