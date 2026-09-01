# Test coverage

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| BASELINE-BUILD | `Pinned Linux build` | supported | Builds source-pinned Hermes and the Godot editor with the external module and tests enabled. | [build script](../../scripts/build_godot.sh) | CI build exit status |
| STYLE-UNIT | `RNViewStyle unit tests` | supported | Covers ARGB conversion, transparent defaults, opacity defaults, overflow defaults, and unsupported colors. | [unit tests](../../modules/react_native_bindings/tests/test_rn_view_style.h) | Godot `--test` exit status |
| BASELINE-SMOKE | `View/Text headless integration` | supported | Rebuilds the shared Metro bundle and verifies the mounted Panel and Label tree, Yoga layout, colors, font size, and opacity. | [manifest](../../samples/view-text/smoke/tests/baseline_mount/smoke_test.json) | `RN_SMOKE_OK: baseline-mount` |
| LAYOUT-SMOKE | `Flex and padding integration` | supported | Verifies that numeric flex fills the mounted root and 16-point padding offsets its direct Label child. | [manifest](../../samples/view-text/smoke/tests/flex_padding/smoke_test.json) | `RN_SMOKE_OK: flex-padding` |
| INTERACTION-UNIT | `Input routing and shadow-node interaction tests` | supported | Covers clone event-target sharing, paint-order hit testing, pointerEvents, clipping, hitSlop, pointer and touch payloads, keyboard activation, and event ordering. | [interaction tests](../../modules/react_native_bindings/tests/test_rn_interaction.h) | Godot `--test` exit status |
| PRESSABLE-SMOKE | `Pressable interaction headless integration` | supported | Exercises Godot mouse, touch, keyboard, focus, hover, capture, bubbling, stopPropagation, cancellation, measurement, layout callbacks, event priorities, state updates, and runtime reload through Pressable. | [manifest](../../samples/view-text/smoke/tests/pressable_interaction/smoke_test.json) | `RN_SMOKE_OK: pressable-interaction` |
| TEST-FABRIC-CONTRACT | `Fabric contract tests` | pending | No dedicated host-method contract suite. | none | none |
| TEST-VISUAL | `Visual regression tests` | pending | No screenshot or image-diff coverage. | none | none |
| TEST-SANITIZER | `Sanitizer and stress tests` | pending | No sanitizer CI coverage. | none | none |
| TEST-CONSUMER | `Consumer and export tests` | pending | No packaged consumer-project coverage. | none | none |
