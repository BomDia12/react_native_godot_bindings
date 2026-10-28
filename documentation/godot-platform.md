# Godot JavaScript platform

The tested platform target is `godot` on the Linux editor/headless build. The JavaScript
platform ships as the `react-native-godot` package in `packages/react-native-godot/`.
Applications depend on it and use its Metro config:

```js
module.exports = require('react-native-godot/metro').createGodotMetroConfig(__dirname);
```

Its Babel preset is `react-native-godot/babel` and its TypeScript base configuration is
`react-native-godot/tsconfig.json`. Bundles run React Native's own `InitializeCore` and then
`react-native-godot/setup` before the main module, so entries need no preamble.
`react-native-godot/setup` installs Godot asset resolution through
`resolveAssetSource.setCustomSourceTransformer`.

The JavaScript project lives in `react_native/project/root` (default `res://react_native/`),
the folder that holds its `package.json`. The samples set it to `res://`. The editor writes
`node_modules/.gdignore` there, so Godot neither imports nor exports installed packages,
while the project's own sources stay visible to the editor.

The package is a React Native Community CLI platform. From the JavaScript project:

```sh
npx react-native build-godot [--entry <file>] [--out <bundle>] [--dev] [--minify]
npx react-native run-godot [--no-packager] [--editor] [--godot <editor>] [-- <Godot arguments>]
```

`build-godot` defaults to `react_native/dev_server/entry` and `react_native/bundle/path`. It
invokes Metro once with `platform: "godot"`, writes a source map and sorted dependency list,
rejects selected `.android.*` and `.ios.*` modules, stages assets, and replaces the bundle's
directory only after validation succeeds. Other bundles in that directory are kept.

`run-godot` uses Metro when `react_native/dev_server/enabled` is on, starting it unless one
answers on the configured host and port, and stops it again with the game. Otherwise, or
with `--no-packager`, it builds the bundle file and passes `--react-native-no-dev-server`,
which makes the runtime load that file even in a project that enables the developer
server. It imports the project once when `.godot/` is missing. The editor is `--godot`,
then `GODOT_EDITOR`, then the prebuilt editor of the installed package version, downloaded
once to `${XDG_CACHE_HOME:-~/.cache}/react-native-godot/<commit>/`.

## Resolution and React Native overrides

Metro uses its normal per-extension order: `.godot`, then `.native`, then the generic
file. This applies independently to `.js`, `.jsx`, `.ts`, and `.tsx`. Unmapped imports
delegate once to Metro's normal Godot resolver; there is no Android or iOS retry.

For a file inside the installed `react-native/Libraries/<path>` or
`react-native/src/private/<path>`, the resolver uses the package's file at the same path
with a `.godot.js` extension when that file exists; file presence is the only rule. A
React Native import that only has Android and iOS files resolves to the Godot file
directly. A same-named application file outside React Native is never
remapped. Bare imports made by the package resolve from the application, so the bundle
always contains one React Native installation.

The `Libraries/` files are of three kinds:

- Platform files implement modules that React Native splits by platform: Platform,
  Image, RCTAlertManager, BaseViewConfig, RCTNetworking, PlatformColorValueTypes,
  BackHandler, legacySendAccessibilityEvent, the DrawerLayoutAndroid and
  ToastAndroid fallbacks, and the React DevTools settings manager.
- Derived overrides are whole-file copies of generic upstream files with a Godot branch:
  Alert, TextInput, TextInputState and Modal. `overrides.json` records the SHA-256 of
  each base file, and `scripts/check-overrides.cjs` fails when the installed React Native
  differs. Re-derive the copy from the new source when that happens.

`Platform.OS` is `godot`. `Platform.select()` checks `godot`, then `native`, then
`default` by property presence, so an explicitly present `undefined` is preserved.
`Platform.constants` comes from the native `PlatformConstants` module and contains:

- `reactNativeVersion`, pinned to 0.87.1;
- `Version` and `godotVersion`, the running Godot semantic version;
- `hostOS` and the debug-build `isTesting` flag;
- `assetScale`, the startup screen scale or `1` in headless mode.

`isTV` and `isVision` are false. These constants are startup values, not live viewport
metrics. TypeScript declarations ship with the package: `godot.d.ts` holds the shared Godot
types and the `react-native` prop augmentations, and `types/` declares each subpath export.
`npm test` type-checks them from a consumer fixture.

## Threads and surfaces

`ReactNativeHost` is an engine singleton that runs one upstream `ReactHost`. JavaScript,
React rendering, layout and text measurement run on its JS thread. Godot nodes and
scene-owned resources are touched only on the main thread, which drains a task queue
from `SceneTree.process_frame`. The main thread never waits on the JS thread. A
synchronous TurboModule call that needs Godot blocks the JS thread until its main-thread
task finishes; while the host shuts down, such calls fail with `E_CANCELLED`. JS timers
use the steady clock, so pausing the SceneTree does not stop them. Interval ticks that
come due while JavaScript is busy are skipped, not queued.

A reload or bundle refresh starts the new runtime at once, and the old runtime is torn
down off the main thread. From that point on, it cannot reach Godot: its queued Godot work
is discarded, its errors are ignored and its synchronous calls fail with `E_CANCELLED`.
It stops its surfaces one at a time and lets React finish unmounting. JavaScript that
keeps it busy for more than 1 s is interrupted.

Animations with `useNativeDriver: true` run in upstream's C++ native Animated driver.
While one is active, the driver steps once per Godot frame on the main thread and applies
its values straight to the Godot controls, so a busy JS thread doesn't stall it. React
Native then commits the final values back to the shadow tree, so later renders keep
them. Native `Animated.event` handlers, such as the one behind ScrollView sticky headers,
read native scroll events on the main thread. Animated layout props, such as `width` or
`top`, go through a Fabric commit and a layout pass on the JS thread.

`ReactNativeHost.evaluate(code)` returns a signal that completes with the converted
result, for example `var value = await ReactNativeHost.evaluate("1 + 1")`.
`get_last_error()` reports the latest JavaScript error and `get_service_stats()`
reports native HTTP, WebSocket and Blob usage.

Upstream C++ logs through glog. The host routes it into Godot's output: errors print as
Godot errors, and info and warnings only show with `--verbose`. glog writes no files.
Debug builds include upstream's LeakChecker. It checks a stopped surface for live
components when the next surface stops, so stopping several surfaces at once would report
components whose React cleanup is still pending. Runtime teardown and roots that leave the
tree together therefore stop their surfaces one at a time, each after React has finished
with the previous one. Before a root's surface stops, events and state updates its
controls sent while leaving the tree are flushed, so they cannot keep its components
alive when the game quits. In development bundles React DevTools keeps stopped surfaces
alive, so LeakChecker reports them there, such as a closed LogBox.

Each `ReactNativeRootView` runs one surface. Surface ids, which React Native calls root
tags, are allocated as `1, 11, 21, …` and never reused. A root starts its surface when it
enters the tree and stops it when it leaves; `reload()` and `application_key` changes
start a new surface. Root size, layout direction, content scale and the
`react_native/text/font_scale` setting become the surface's layout constraints. A root's
`reload()` and `application_key` changes restart only that surface, on the current
runtime.

A hard reload replaces the runtime and restarts every live root. It comes only from
`ReactNativeHost.reload()`, from a bundle change signalled by `ReactNativeFileSingleton`,
from JavaScript's `DevSettings.reload()`, which only `__DEV__` bundles call through, or
from the developer server's reload requests. Each
runtime runs one bundle. `force_refresh()` always reloads, even when the file is
unchanged, and so does a new `set_monitored_file()` path. Only the editor process watches
the bundle file for changes; a running game or an exported build doesn't. A bundle that is
missing when roots start leaves them empty, and the bundle-change signal that makes it
available starts the runtime and every waiting root.

## Developer server

With `react_native/dev_server/enabled` on, each runtime downloads its bundle from
`http://<host>:<port>/<entry>.bundle?platform=godot&dev=true&…` through the Godot HTTP
service, caches it under `user://react_native/cache/downloads/`, and loads it under its
Metro URL; `ResourceLoader` resolves that URL to the cached copy. Surfaces that start
meanwhile wait for the bundle. When the download fails, the runtime loads
`react_native/bundle/path`. Export templates ignore the setting. The bundle must stay
within `react_native/network/http/max_body_bytes`.

Upstream `ReactHost` stays out of dev mode, because its dev mode asks Metro for an Android
bundle and reloads the runtime in place. Godot instead reuses upstream's pieces:

- The `SourceCode` module reports the Metro URL, so LogBox symbolication and Fast Refresh
  find Metro. `react-native-godot/setup` starts upstream's Fast Refresh client for a
  bundle Metro served.
- Upstream's packager connection listens on Metro's `/message` socket. Its reload request
  goes to `ReactNativeHost.reload()`; Godot has no developer menu.
- Upstream's inspector registers the runtime with Metro's DevTools proxy. Godot's
  DevTools socket answers DevTools page reloads itself and reloads through
  `ReactNativeHost.reload()`. Each reload registers a new inspector page, so DevTools
  reconnects to it.
- `react-native-godot-start` also answers `GET /status` with `packager-status:running`, as
  React Native's own server does, and `POST /reload` with the reload its `r` key sends.
- LogBox opens its inspector in its own `ReactNativeRootView` with the `LogBox`
  application key, on a `CanvasLayer` above every other layer. A reload closes it.

Debug builds report `Platform.isTesting`, which turns LogBox notifications off, so only
uncaught errors open it there unless the app calls `LogBox.ignoreAllLogs(false)`. With the
inspector on, warnings go to the DevTools console instead of LogBox, as upstream.

## Colors

Component colors use React Native's own `processColor` and C++ color parsing. Values that
cross into Godot scripts through `react-native-godot/scene` use explicit wrappers such
as `{$godot: 'Color', r: 0.2, g: 0.4, b: 0.6, a: 1}`; see [interop](interop.md).

## Local assets

Metro-reported variants are copied to `assets/<metro-content-hash>/<scale>.<extension>`
beside the bundle, for example `res://react_native/dist/assets/…`. The generated manifest
preserves logical name, type, dimensions, scales, hashes, and ordered paths relative to the
bundle, without host paths or timestamps. A bundle file reports its `res://` path as
`SourceCode.scriptURL`, and assets resolve next to it. A bundle that Metro served gets its
assets from Metro, as on other platforms, so assets added while developing need no rebuild. At runtime the resolver chooses the smallest scale at least as
large as `assetScale`, or the largest available, and returns React Native's local asset
shape.

Explicit `res://` and `user://` objects are supported for local operations. PNG/JPEG/static WebP and base64 images use the bounded native image service;
getSize, prefetch and queryCache go through upstream's `ImageLoader` module, backed by the
same service. HTTP images use the shared bounded HTTPRequest pool. Prefetch cannot be
aborted, as in upstream's C++ image loader. The native image service requires the exact
scheme, forward slashes, a nonempty relative path, and no `..` segment or embedded NUL.
Percent characters remain literal. Missing staged files should be fixed by rebuilding
the bundle.

## Pending APIs

Imports of pending adapters are safe, but attempted operations report `E_UNSUPPORTED`.
`BackHandler.exitApp`, DrawerLayoutAndroid, ProgressBarAndroid, and ToastAndroid (upstream's
non-Android fallbacks) are not claimed as
supported services or components. Godot reports no hardware back presses, so, as on iOS,
`BackHandler` listeners return a subscription and never fire.

## Native components

The TextInput override preserves upstream state synchronization and ref methods and
selects the unified Godot host for both editor modes; the Modal override retains Modal
until its native dismiss event.

React Native flattens Views that only lay out, and it mounts a View that only draws
without nesting its children, so a Text can be a sibling of its bordered parent View in
the Godot tree. Views that clip, transform, change opacity or set `pointerEvents` to `none` or `box-only` keep
their children nested; `pointerEvents="none"` disables input for that Godot subtree.
A press that opens a Godot popup, such as a `godotContextMenu` or a text input's context
menu, hands the release to the popup, so the root cancels that mouse pointer
(`pointercancel`, plus `touchcancel` for a primary press).

Text inherits the root's Godot RichTextLabel theme font, size and color. The Text host
shapes, wraps and draws through TextServer itself, with per-span colors, decorations in
their own color and style, shadows and line heights, over the Text view's own painted
box, inside the Text view's padding and borders. The tallest span line height sets each line box. `numberOfLines` trims the last
visible line with a tail ellipsis; a single line can also take a head or middle ellipsis,
cut at grapheme boundaries, and hit testing maps back across the hidden text.
`adjustsFontSizeToFit` searches hundredths of the font scale down to `minimumFontScale`.
Natural, start and end alignment follow the layout direction, `left` and `right` swap in
right-to-left layouts as upstream does, and the base direction is
`writingDirection` or content-derived. A font's weight, style, spacing and `fontVariant`
features apply to its fallback fonts too. Register
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
behavior is not covered by the automated replay test. Keyboard events reflect actual DisplayServer virtual-keyboard geometry; desktop focus does not synthesize visibility. Pointer eligibility applies to the wrapper and active/staged editors
without changing editable visuals. autoFocus runs once, when the input first enters the tree. Unsupported secure multiline input fails validation.

ScrollView owns a native ScrollContainer and a separate React content host. The
scroll axis measures independently, indicator reservations use native theme metrics,
and offsets are integral and clamp when content shrinks. Native drag/deceleration
supplies phase events; native Tweens implement animated commands and selected
snap/paging behavior. A new user gesture cancels the programmatic Tween. Pointer modes also govern
native scrollbars, while box-none retains interactive React children. Visible
content anchoring uses retained row identities; a changed contentOffset wins.

Modal uses an overlay in its nearest presentation window. Presenting pushes a
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

## View painting

A View paints through `RNStyleBox`, a module StyleBox built from upstream's resolved
border metrics: background color over the border box, background gradients tiled over
the padding box and clipped to the border box, per-edge borders joined diagonally at
corners, dashed and dotted edges, and the outline. Edges are antialiased with a
half-pixel feathered ring. Box shadows are two canvas items under the View's own:
outset shadows behind it and inset shadows below its children.

`overflow: 'hidden'` moves the View's children into an internal clip host. A square
host clips with Godot's rectangle clipping. A rounded host carries a clip material that
its descendants inherit through `use_parent_material`; the shader multiplies coverage
by every rounded clip above it, so clips nest and compose with filters, blend modes and
rounded images, which put the same stack into their own shaders. Up to eight rounded
clips apply. Square clips use Godot's rectangle clipping, narrowed to the same padding box. Hit testing ignores points outside a rounded clip.

`filter`, `mixBlendMode` and `isolation: 'isolate'` render the View's subtree through an
internal effect host, a SubViewportContainer whose SubViewport holds the painted box and
its children. Filters run as a canvas group shader there, over a nested viewport that holds the subtree, because Godot copies no backdrop for blending while a canvas group is open; blend modes run in the
container's shader against a copy of the viewport drawn so far. Godot maps input
through the container, and focus moves into the subtree and out of it in Tab order.
Godot reports only the window viewport's focus owner to screen readers, so focus inside
an effect host is announced as the window.

Upstream resolves transforms around the View's center, as iOS and Android layers do,
so the stock Control offset transform pivots there. Yoga frames are already mirrored for
right-to-left layouts, so component controls stay left-to-right in Godot and do not
mirror again; TextInput editors fill their host and take the layout direction. A
View's `cursor` sets its Godot cursor shape and its React Native descendants inherit
it where theirs is `auto`.

## Accessibility

Godot's AccessKit integration carries React Native accessibility. `accessibilityLabel`,
`accessibilityHint` and `accessibilityLiveRegion` (and their `aria-*` forms) set the
focus control's own `accessibility_name`, `accessibility_description` and
`accessibility_live`. A View adds its role, states, value and actions in
`NOTIFICATION_ACCESSIBILITY_UPDATE`: React Native roles map to Godot roles, roles without
an equivalent publish a container with the React Native role as its description, `none`
and `presentation` keep the view's own panel role, and `mixed` sends
unchecked with the state description `mixed`, because Godot's checked state
is boolean. Hidden subtrees and modal views set `FLAG_HIDDEN` and `FLAG_MODAL`. An
`accessible` View is a single element, as on iOS and Android: its descendants sit under a
hidden sub-element, as TabContainer hides inactive tabs, so screen readers skip them while
AccessKit keeps them in the tree; an open context menu stays outside it. With no label, it is named by the text inside it. The screen reader's click
sends the same `onKeyboardClick` a keyboard press does, or the `activate` action when one is
listed; `increment`, `decrement`, `expand` and `collapse` use Godot's own actions, and other
actions become custom actions. Text publishes `link` and `heading` roles and one text run
per line for selection. Switch, TextInput, ScrollView and Godot windows keep their native
roles, and Modal publishes a modal dialog.

`AccessibilityInfo` uses the `AccessibilityManager` module that upstream's non-Android
path calls. Announcements go through the root window's `accessibility_announcement`, and
`setAccessibilityFocus` focuses the view's focus control. Tab and screen-reader order are
Godot node order, which follows React order.

## Animation

`LayoutAnimation` runs upstream's `LayoutAnimationDriver`. The mounting manager keeps the
component registry the Scheduler builds through it, installs the driver as the UIManager's
animation delegate in `setUIManager`, and makes it the mounting override delegate of every
surface that starts. `RNHostInstance::process_frame` ticks it before draining the main-thread
queue, so interpolated mutations mount in the frame they are computed. Native `Animated`
keeps its own per-frame callback. JS-driven animations follow upstream's C++
`requestAnimationFrame`, which is `setTimeout(0)`.

## Application services

Dimensions/PixelRatio describe the application Window and screen, independently of
small RN roots. Window resize/scale changes coalesce before JS delivery. `fontScale`
reaches native Text/TextInput measurement and rendering as well as public metrics.
Scaling is inherited through Text spans; `allowFontScaling=false` disables it and
`maxFontSizeMultiplier` null inherits, zero is unlimited, and values ≥1 cap scaling.
Native sizes round once after scaling; explicit lineHeight uses the same multiplier.
`react_native/text/font_scale` can change at runtime.

Appearance is application-controlled (`light`/`dark`); `unspecified` clears the override
and restores the configured default or enabled host preference. `follow_system=false`
by default.
AppState tracks application pause/resume and focus independently of SceneTree pause.
I18nManager direction preferences persist in `user://react_native_direction.cfg` and
apply on restart/bundle generation. Native authored layout direction remains available.
RN zIndex orders siblings in one surface; Godot nodes/windows order separate roots.

`Alert.alert` goes through upstream's `AlertManager` module to the application handler
set with `GodotAlerts.set_alert_handler`, or to a native dialog when none is installed.
GodotAppRegistry registration can opt a root into a native Alert presenter or a custom JS
presenter for `useGodotRoot().alert`. Roots without one, and explicit declines, bubble to
the root's Godot handler and then the application handler. Alerts from a root that has
stopped are canceled. See [extension APIs](extensions.md). Modal and Window
continue to use the native controls; headless Window evidence
proves logical content/lifetime only. SafeAreaView keeps the non-iOS View fallback;
RefreshControl is pending; use an explicit Refresh button for network data.

HTTP/fetch/XHR support complete responses, status, cancellation/timeout, redirects,
verified TLS (trust a custom certificate with `react_native/network/tls/trusted_certificates`), decompression, multipart strings/Blob/local files, binary byte views and
bounded in-memory host-only cookies; Domain attributes are rejected. XHR receives response/data/completion in order after native
completion; incremental responseText/progress streaming is outside this subset.
Text decoding preserves NUL and replaces invalid UTF-8. Blob and FileReader reads use
bounded native chunks and explicit/GC ownership. WebSocketPeer handles framing,
subprotocols, text/binary and graceful CLOSING with a bounded deadline; no automatic
reconnect is provided.

Clipboard delegates to DisplayServer. Linking.openURL delegates to the host opener;
initial URL is null without a host delivery provider, and canOpenURL/openSettings report
unsupported without a capability provider. Vibration requires a handheld host provider;
desktop controllers are not selected. Keyboard geometry is reported only when the
backend provides it. Broader mobile/export fidelity remains pending.
