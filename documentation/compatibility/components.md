# Component compatibility

Public cross-platform component exports from React Native 0.87.1 are classified below.
`RCTRawText` is an internal host primitive folded into `Text`, not a public component
export. Components that React Native scopes to a single platform are out of scope and are
not listed: `DrawerLayoutAndroid`, `InputAccessoryView`, `ProgressBarAndroid`,
`SafeAreaView`, and `TouchableNativeFeedback`. `Touchable` remains a runtime compatibility
re-export but is absent from the tagged public types.

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| COMP-ACTIVITY-INDICATOR | `ActivityIndicator` | pending | No native primitive. | none | none |
| COMP-BUTTON | `Button` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-FLAT-LIST | `FlatList` | pending | Native scrolling is absent; public list virtualization and measurement behavior are not verified. | none | none |
| COMP-IMAGE | `Image` | pending | No native image primitive. | none | none |
| COMP-IMAGE-BACKGROUND | `ImageBackground` | pending | Depends on `Image`. | none | none |
| COMP-KEYBOARD-AVOIDING | `KeyboardAvoidingView` | pending | Keyboard and layout integration are absent. | none | none |
| COMP-LAYOUT-CONFORMANCE | `experimental_LayoutConformance` | pending | Experimental export is not verified. | none | none |
| COMP-MODAL | `Modal` | pending | No native modal primitive. | none | none |
| COMP-NATIVE-TEXT | `unstable_NativeText` | pending | Underlying host primitive exists but this public export is not verified. | none | none |
| COMP-NATIVE-VIEW | `unstable_NativeView` | pending | Underlying host primitive exists but this public export is not verified. | none | none |
| COMP-PRESSABLE | `Pressable` | partially supported | Mouse, touch, keyboard, focus, hover, press, responder, capture, and bubble paths work through Godot input routing. Pressable composes retained View hosts, so measurement and pointer capture use the shared host-ref implementation; broader ref behavior, long-press and press-retention edge cases, production feature flags, and broader platform coverage remain unverified. | [input router](../../modules/react_native_bindings/input/rn_input_router.cpp), [event bridge](../../modules/react_native_bindings/fabric/fabric_ui_manager.cpp), [mounting manager](../../modules/react_native_bindings/mounting/rn_mounting_manager.cpp), [NativeDOM host methods](../../modules/react_native_bindings/fabric/native_dom.cpp) | [PRESSABLE-SMOKE](test-coverage.md), [MULTI-ROOT-SMOKE](test-coverage.md), [TRANSACTION-MOUNT-SMOKE](test-coverage.md) |
| COMP-REFRESH-CONTROL | `RefreshControl` | pending | Scrolling and refresh behavior are absent. | none | none |
| COMP-SCROLL-VIEW | `ScrollView` | pending | No scrolling primitive or retained position. | none | none |
| COMP-SECTION-LIST | `SectionList` | pending | Native scrolling is absent; public section-list virtualization and measurement behavior are not verified. | none | none |
| COMP-STATUS-BAR | `StatusBar` | pending | No Godot adaptation is defined. | none | none |
| COMP-SWITCH | `Switch` | pending | No native switch primitive. | none | none |
| COMP-TEXT | `Text` | partially supported | Mounts one Godot `Label`; retained transactional commits preserve the tested host identity through text updates, reorders, insertions, rollback, recovery, and resize. A public host ref exposes the tested tag/text and DOM-compatible traversal/geometry subset, while nested spans, selection, bidi, and full typography are absent. | [mounting manager](../../modules/react_native_bindings/mounting/rn_mounting_manager.cpp), [root view](../../modules/react_native_bindings/root_view/react_native_root_view.cpp), [NativeDOM host methods](../../modules/react_native_bindings/fabric/native_dom.cpp) | [BASELINE-SMOKE](test-coverage.md), [MULTI-ROOT-SMOKE](test-coverage.md), [TRANSACTION-MOUNT-SMOKE](test-coverage.md) |
| COMP-TEXT-ANCESTOR | `unstable_TextAncestorContext` | pending | Public JS context export is not verified. | none | none |
| COMP-TEXT-INPUT | `TextInput` | pending | Text input, selection, and IME integration are not implemented. | none | none |
| COMP-TOUCHABLE-HIGHLIGHT | `TouchableHighlight` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-TOUCHABLE-OPACITY | `TouchableOpacity` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-TOUCHABLE-WITHOUT | `TouchableWithoutFeedback` | pending | The public component is not verified against the routed interaction path. | none | none |
| COMP-VIEW | `View` | partially supported | Mounts a Godot `Panel` with Yoga layout, visual styles, pointer hit testing, layout events, and routed mouse, touch, keyboard, focus, and hover events; retained transactional mounting preserves tested host identity and published layout/input snapshots across updates, reorders, rollback, recovery, and resize. Pointer hit testing respects Godot visibility for roots, ancestors, and hosts. Public host refs expose the tested tag, text/traversal, geometry, and native-props subset. Accessibility, transforms, broader composite-ref behavior, and persistent native identity across reloads remain Godot limitations. | [mounting manager](../../modules/react_native_bindings/mounting/rn_mounting_manager.cpp), [root view](../../modules/react_native_bindings/root_view/react_native_root_view.cpp), [NativeDOM host methods](../../modules/react_native_bindings/fabric/native_dom.cpp), [input router](../../modules/react_native_bindings/input/rn_input_router.cpp) | [BASELINE-SMOKE](test-coverage.md), [INTERACTION-UNIT](test-coverage.md), [PRESSABLE-SMOKE](test-coverage.md), [MULTI-ROOT-SMOKE](test-coverage.md), [TRANSACTION-MOUNT-SMOKE](test-coverage.md), [MOUNTING-REGRESSION-SMOKE](test-coverage.md) |
| COMP-VIRTUALIZED-LIST | `VirtualizedList` | pending | Native scrolling is absent; public virtualization and list measurement behavior are not verified. | none | none |
| COMP-VIRTUALIZED-SECTION | `VirtualizedSectionList` | pending | Native scrolling is absent; public section-list virtualization and measurement behavior are not verified. | none | none |
| COMP-VIRTUAL-VIEW | `unstable_VirtualView` | pending | Experimental component is not implemented or verified. | none | none |
| COMP-VIRTUAL-COLUMN | `unstable_VirtualColumn` | pending | Experimental virtual-collection component is not implemented or verified. | none | none |
| COMP-VIRTUAL-ROW | `unstable_VirtualRow` | pending | Experimental virtual-collection component is not implemented or verified. | none | none |
