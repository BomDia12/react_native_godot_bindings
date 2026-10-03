# Godot JavaScript platform

The tested platform target is `godot` on the Linux editor/headless build. The sample
config calls `createGodotMetroConfig(projectRoot)` from `js/godot/metro.cjs`, and its
bundle command is:

```sh
npm --prefix samples/view-text run build:godot
```

The builder invokes Metro once with `platform: "godot"`, writes a source map and sorted
dependency list, rejects selected `.android.*` and `.ios.*` modules, stages assets, and
publishes the completed `dist/` directory only after validation succeeds.

## Resolution and React Native overrides

Metro uses its normal per-extension order: `.godot`, then `.native`, then the generic
file. This applies independently to `.js`, `.jsx`, `.ts`, and `.tsx`. Unmapped imports
delegate once to Metro's normal Godot resolver; there is no Android or iOS retry.

`js/godot/rn-overrides.cjs` maps exact files inside the installed React Native package.
The current adapters cover Platform, color processing, BaseViewConfig, local asset
resolution, Image, BackHandler, networking, alerts, legacy accessibility sending,
DrawerLayoutAndroid, ProgressBarAndroid, and ToastAndroid. This map is coupled to React
Native 0.87.1 and must be re-audited when that pin changes. A same-named application file
outside that package is never remapped.

`Platform.OS` is `godot`. `Platform.select()` checks `godot`, then `native`, then
`default` by property presence, so an explicitly present `undefined` is preserved.
`Platform.constants` comes from the native `PlatformConstants` module and contains:

- `reactNativeVersion`, pinned to 0.87.1;
- `Version` and `godotVersion`, the running Godot semantic version;
- `hostOS` and the debug-build `isTesting` flag;
- `assetScale`, the startup screen scale or `1` in headless mode.

`isTV` and `isVision` are false. These constants are startup values, not live viewport
metrics. Godot-specific TypeScript declarations are in `js/godot/godot.d.ts`.

## Colors

Public React Native color inputs still use `@react-native/normalize-colors`. The adapter
immediately converts its `0xRRGGBBAA` result to an explicit wrapper:

```js
{$godot: 'Color', r: 0.2, g: 0.4, b: 0.6, a: 1}
```

All channels must be finite and in `0..1`. Native code constructs `Color` directly;
packed ARGB is not a supported wire format. Null removes a color prop. Platform,
dynamic, and theme colors currently fail with `E_UNSUPPORTED` rather than choosing an
arbitrary fallback.

## Local assets

Metro-reported variants are copied to
`res://dist/assets/<metro-content-hash>/<scale>.<extension>`. The generated manifest
preserves logical name, type, dimensions, scales, hashes, and ordered URIs without host
paths or timestamps. At runtime the resolver chooses the smallest scale at least as
large as `assetScale`, or the largest available, and returns React Native's local asset
shape.

Explicit `res://` and `user://` objects are supported for local operations. PNG/JPEG/static WebP and base64 images use the bounded native image service;
getSize, prefetch and queryCache share its source and credential policy. HTTP images
require an installed RNImageTransport; the real Phase 6B transport is deferred. Paths must use the exact
scheme, forward slashes, a nonempty relative path, and no `..` segment or embedded NUL.
Percent characters remain literal. Missing staged files should be fixed by rebuilding
the bundle.

## Pending APIs

Imports of pending adapters are safe, but attempted operations report `E_UNSUPPORTED`.
Network requests, alerts, BackHandler listeners, legacy accessibility
dispatch, DrawerLayoutAndroid, ProgressBarAndroid, and ToastAndroid are not claimed as
supported services or components. Bootstrap shims exist only for React Native startup
dependencies and are not compatibility claims.

## Native components and upstream source adaptations

`source-adaptations.cjs` changes only exact installed RN 0.87.1 paths and rejects a
source digest mismatch. It preserves upstream TextInput state synchronization and
ref methods, selects the unified Godot host for both editor modes, retains Modal
until its native dismiss event, and feeds sticky-header Animated values from native
scroll events. The Animated props hook skips the unavailable native queue on Godot.
Button opacity feedback explicitly uses the upstream JS animation driver on Godot.
General NativeAnimated support is absent. Lists currently use the existing bootstrap
queue; real scheduler integration is deferred.

Text inherits the root's Godot RichTextLabel theme font, size and color. Register
Font resources or res:// font paths under `react_native/text/font_aliases`; an alias
record may contain `font`, `weight` and `style`. Metro fonts use
`fontFamily(require('./font.woff2'))` from `react-native-godot/fonts`. TTF, OTF,
WOFF and WOFF2 are staged and imported by Godot. FontVariation instances isolate
per-run weight, style and spacing. Native font sizes and spacing round to integers.

Use controlled TextInput `value` with `onChangeText`; updates require the current
native event count. Use `defaultValue` for uncontrolled editor state. Native
LineEdit/TextEdit own Unicode input, selection, undo, clipboard and IME; controlled
replacement waits while composing. Switching multiline retains the wrapper and
value/selection while starting a new native editor history. OS candidate-window
behavior is not covered by the automated replay test. Keyboard service integration
is deferred. Pointer eligibility applies to the wrapper and active/staged editors
without changing editable visuals. autoFocus runs once after initial publication. Unsupported secure multiline input fails validation.

ScrollView owns a native ScrollContainer and a separate React content host. The
scroll axis measures independently, indicator reservations use native theme metrics,
and offsets are integral and clamp when content shrinks. Native drag/deceleration
supplies phase events; native Tweens implement animated commands and selected
snap/paging behavior. A new user gesture cancels the programmatic Tween. Pointer modes also govern
native scrollbars, while box-none retains interactive React children. Visible
content anchoring uses retained row identities; a changed contentOffset wins.

Modal uses an overlay in its nearest presentation window. Publication pushes a
hidden-to-visible modal onto its root/window stack; rerenders retain stack order.
Escape requests closure through onRequestClose. GodotWindow is imported from
`react-native-godot/GodotWindow`, accepts width/height/title/visible and
onShow/onDismiss/onRequestClose/onResize, and owns an actual native Window.
Window sizes are native integer pixels. User resizing survives unrelated renders.

ActivityIndicator maps to Godot's indeterminate ProgressBar; it is a linear native
indicator; color alpha combines with style opacity. Switch maps to CheckButton with controlled native state; platform track
appearance is not reproduced. Switch pointer handling honors its own and inherited
pointerEvents without changing enabled visuals; native focus/keyboard behavior
remains independent. Full compatibility classifications remain partial.
