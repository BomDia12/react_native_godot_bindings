# Test coverage

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| BASELINE-BUILD | `Pinned Linux build` | supported | Builds source-pinned Hermes and the Godot editor with the external module and tests enabled. | [build script](../../scripts/build_godot.sh) | CI build exit status |
| STYLE-UNIT | `RNViewStyle unit tests` | supported | Covers ARGB conversion, transparent defaults, opacity defaults, overflow defaults, and unsupported colors. | [unit tests](../../modules/react_native_bindings/tests/test_rn_view_style.h) | Godot `--test` exit status |
| BASELINE-SMOKE | `View/Text headless integration` | supported | Rebuilds the Metro bundle and verifies the Fabric commit, native tree, layout, colors, font size, and opacity. | [smoke gate](../../samples/view-text/smoke/smoke_gate.gd) | `RN_BASELINE_OK` |
| TEST-FABRIC-CONTRACT | `Fabric contract tests` | pending | No dedicated host-method contract suite. | none | none |
| TEST-VISUAL | `Visual regression tests` | pending | No screenshot or image-diff coverage. | none | none |
| TEST-SANITIZER | `Sanitizer and stress tests` | pending | No sanitizer CI coverage. | none | none |
| TEST-CONSUMER | `Consumer and export tests` | pending | No packaged consumer-project coverage. | none | none |
