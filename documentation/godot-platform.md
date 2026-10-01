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

Explicit `res://` and `user://` objects are supported for local operations. Remote/data
Image loading, prefetch, and image sizing remain pending. Paths must use the exact
scheme, forward slashes, a nonempty relative path, and no `..` segment or embedded NUL.
Percent characters remain literal. Missing staged files should be fixed by rebuilding
the bundle.

## Pending APIs

Imports of pending adapters are safe, but attempted operations report `E_UNSUPPORTED`.
Image rendering, network requests, alerts, BackHandler listeners, legacy accessibility
dispatch, DrawerLayoutAndroid, ProgressBarAndroid, and ToastAndroid are not claimed as
supported services or components. Bootstrap shims exist only for React Native startup
dependencies and are not compatibility claims.
