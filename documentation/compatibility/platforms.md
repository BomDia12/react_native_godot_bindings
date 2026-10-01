# Platform compatibility

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| PLATFORM-LINUX-EDITOR | `Linux editor` | supported | Pinned Godot editor and external module build in CI. | [workflow](../../.github/workflows/linux-baseline.yml) | [BASELINE-BUILD](test-coverage.md) |
| PLATFORM-LINUX-HEADLESS | `Linux headless` | supported | Bundles and mounts the semantic View/Text fixture without a display server. | [smoke runner](../../scripts/run_smoke_tests.py) | [BASELINE-SMOKE](test-coverage.md) |
| PLATFORM-METRO-ANDROID | `Godot Metro compatibility path` | adapted for Godot | The tested Linux Godot path builds and consumes the Godot-targeted Metro bundle (`--platform godot`) and decodes explicit RGBA colors; this is a Godot adaptation, not an Android export. | [Metro config](../../samples/view-text/metro.config.godot.js), [bundle script](../../scripts/build_godot_bundle.cjs), [smoke runner](../../scripts/run_smoke_tests.py) | [BASELINE-SMOKE](test-coverage.md), [JS-ADAPTER-UNIT](test-coverage.md) |
| PLATFORM-WINDOWS | `Windows` | pending | Not built or tested. | none | none |
| PLATFORM-MACOS | `macOS` | pending | Not built or tested. | none | none |
| PLATFORM-IOS | `iOS` | pending | Not built or tested. | none | none |
| PLATFORM-ANDROID | `Android` | pending | Godot Android export is not built or tested. | none | none |
| PLATFORM-WEB | `Web` | pending | Requires a separate platform strategy. | none | none |
