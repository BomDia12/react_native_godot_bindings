# Platform compatibility

| ID | Surface | Status | Behavior / limitations | Implementation evidence | Test evidence |
|---|---|---|---|---|---|
| PLATFORM-LINUX-EDITOR | `Linux editor` | supported | Pinned Godot editor and external module build in CI. | [workflow](../../.github/workflows/linux-baseline.yml) | [BASELINE-BUILD](test-coverage.md) |
| PLATFORM-LINUX-HEADLESS | `Linux headless` | supported | Bundles and mounts the semantic View/Text fixture without a display server. | [smoke runner](../../scripts/run_smoke_tests.py) | [BASELINE-SMOKE](test-coverage.md) |
| PLATFORM-METRO-ANDROID | `Android Metro compatibility path` | adapted for Godot | The Godot runtime temporarily consumes an Android-targeted bundle and signed-ARGB colors. | [bundle script](../../samples/view-text/package.json) | [BASELINE-SMOKE](test-coverage.md) |
| PLATFORM-WINDOWS | `Windows` | pending | Not built or tested. | none | none |
| PLATFORM-MACOS | `macOS` | pending | Not built or tested. | none | none |
| PLATFORM-IOS | `iOS` | pending | Not built or tested. | none | none |
| PLATFORM-ANDROID | `Android` | pending | Godot Android export is not built or tested. | none | none |
| PLATFORM-WEB | `Web` | pending | Requires a separate platform strategy. | none | none |
