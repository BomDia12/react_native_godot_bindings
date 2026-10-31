# Platform compatibility

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| PLATFORM-LINUX-EDITOR | `Linux editor` | supported | Pinned Godot editor and external module build in CI, with the example library's native module autolinked. `react-native build-godot-editor` builds the same editor for a project's linked native modules. | [workflow](../../.github/workflows/linux-baseline.yml) | [BASELINE-BUILD](test-coverage.md) |
| PLATFORM-LINUX-HEADLESS | `Linux headless` | supported | Runs renderer regressions, the retained upstream gallery and the script-authoritative game without a display server. Native Linux display behavior has a separate Xvfb/software OpenGL gate; OS IME candidates remain unverified. | [smoke runner](../../scripts/run_smoke_tests.py) | [BASELINE-SMOKE](test-coverage.md), [COMPONENT-CONTRACTS](test-coverage.md), [COMPONENT-DISPLAY](test-coverage.md) |
| PLATFORM-METRO-ANDROID | `Godot Metro compatibility path` | adapted for Godot | The tested Linux Godot path builds and consumes the Godot-targeted Metro bundle (`--platform godot`) and decodes explicit RGBA colors; this is a Godot adaptation, not an Android export. | [Metro config](../../packages/react-native-godot/metro.cjs), [bundle builder](../../packages/react-native-godot/cli/build.cjs), [smoke runner](../../scripts/run_smoke_tests.py) | [BASELINE-SMOKE](test-coverage.md), [JS-ADAPTER-UNIT](test-coverage.md) |
| PLATFORM-WINDOWS | `Windows` | pending | Not built or tested. | none | none |
| PLATFORM-MACOS | `macOS` | pending | Not built or tested. | none | none |
| PLATFORM-IOS | `iOS` | pending | Not built or tested. | none | none |
| PLATFORM-ANDROID | `Android` | pending | Godot Android export is not built or tested. | none | none |
| PLATFORM-WEB | `Web` | pending | Requires a separate platform strategy. | none | none |
