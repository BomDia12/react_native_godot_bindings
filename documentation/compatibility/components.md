# Component compatibility

Public component exports from React Native 0.84.1 are classified below. `RCTRawText` is
an internal host primitive folded into `Text`, not a public component export.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| COMP-ACTIVITY-INDICATOR | `ActivityIndicator` | pending | No native primitive. | none | none |
| COMP-BUTTON | `Button` | pending | Requires the Phase 3 interaction path. | none | none |
| COMP-DRAWER-ANDROID | `DrawerLayoutAndroid` | pending | Android-specific component has no Godot decision. | none | none |
| COMP-FLAT-LIST | `FlatList` | pending | Requires scrolling and durable identity. | none | none |
| COMP-IMAGE | `Image` | pending | No native image primitive. | none | none |
| COMP-IMAGE-BACKGROUND | `ImageBackground` | pending | Depends on `Image`. | none | none |
| COMP-INPUT-ACCESSORY | `InputAccessoryView` | pending | No keyboard accessory equivalent is defined. | none | none |
| COMP-KEYBOARD-AVOIDING | `KeyboardAvoidingView` | pending | Keyboard and layout integration are absent. | none | none |
| COMP-LAYOUT-CONFORMANCE | `experimental_LayoutConformance` | pending | Experimental export is not verified. | none | none |
| COMP-MODAL | `Modal` | pending | No native modal primitive. | none | none |
| COMP-NATIVE-TEXT | `unstable_NativeText` | pending | Underlying host primitive exists but this public export is not verified. | none | none |
| COMP-NATIVE-VIEW | `unstable_NativeView` | pending | Underlying host primitive exists but this public export is not verified. | none | none |
| COMP-PRESSABLE | `Pressable` | pending | Rendering works through `View`; input does not. | none | none |
| COMP-PROGRESS-ANDROID | `ProgressBarAndroid` | pending | Deprecated Android-specific component has no Godot decision. | none | none |
| COMP-REFRESH-CONTROL | `RefreshControl` | pending | Scrolling and refresh behavior are absent. | none | none |
| COMP-SAFE-AREA | `SafeAreaView` | pending | Safe-area behavior is absent. | none | none |
| COMP-SCROLL-VIEW | `ScrollView` | pending | No scrolling primitive or retained position. | none | none |
| COMP-SECTION-LIST | `SectionList` | pending | Depends on `ScrollView` and durable identity. | none | none |
| COMP-STATUS-BAR | `StatusBar` | pending | No Godot adaptation is defined. | none | none |
| COMP-SWITCH | `Switch` | pending | No native switch primitive. | none | none |
| COMP-TEXT | `Text` | partially supported | Mounts one Godot `Label`; nested spans, selection, bidi, and full typography are absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| COMP-TEXT-ANCESTOR | `unstable_TextAncestorContext` | pending | Public JS context export is not verified. | none | none |
| COMP-TEXT-INPUT | `TextInput` | pending | Requires durable identity, focus, selection, and IME support. | none | none |
| COMP-TOUCHABLE | `Touchable` | pending | Input and responder behavior are absent. | none | none |
| COMP-TOUCHABLE-HIGHLIGHT | `TouchableHighlight` | pending | Input and responder behavior are absent. | none | none |
| COMP-TOUCHABLE-NATIVE | `TouchableNativeFeedback` | pending | Android-specific feedback has no Godot decision. | none | none |
| COMP-TOUCHABLE-OPACITY | `TouchableOpacity` | pending | Input and responder behavior are absent. | none | none |
| COMP-TOUCHABLE-WITHOUT | `TouchableWithoutFeedback` | pending | Input and responder behavior are absent. | none | none |
| COMP-VIEW | `View` | partially supported | Mounts a Godot `Panel` with basic Yoga layout and visual styles; events, accessibility, transforms, and persistent identity are absent. | [renderer](../../modules/react_native_bindings/root_view/react_native_root_view.cpp) | [BASELINE-SMOKE](test-coverage.md) |
| COMP-VIRTUALIZED-LIST | `VirtualizedList` | pending | Requires scrolling, measurement, and durable identity. | none | none |
| COMP-VIRTUALIZED-SECTION | `VirtualizedSectionList` | pending | Requires scrolling, measurement, and durable identity. | none | none |
| COMP-VIRTUAL-VIEW | `unstable_VirtualView` | pending | Experimental component is not implemented or verified. | none | none |
