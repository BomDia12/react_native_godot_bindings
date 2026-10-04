# Native services and game validation

The Linux editor/headless application gate passed locally on 2026-10-04. The
versioned application is `samples/game-ui`; its built standalone copy is staged at
`../godotProjects/Phase6BGame`. Three GDScript enemies and a Godot-owned inventory
drive four independent RN surfaces through declared script capabilities. Direct
and repeated frame updates work with no server and start zero HTTP requests.

## Provenance

The base pins in `baseline.env` are unchanged: Godot
`ed1daf0bf001b61586d9930840f2f1394092c079`, Hermes
`3477757eb2475555cf8d8df24bfb1deb0613880d` (`250829098.0.17`), React Native
`0.87.1`, React `19.2.3`, Metro `0.87.0`, and Yoga's recorded RN revision
`a59eff64fa907ed6e919fafe6cbd26d1d54c2de3`.

The existing additive Godot patch set is unchanged, with identity
`8a31c29161c6326aa93ae6bf1a3f7e4e4605d5ca29ecc757122808711234bd25`.
Local gates used the verified `.phase6a/godot` worktree through
`GODOT_SOURCE_DIR`, leaving unrelated changes in the default engine checkout
untouched. Provenance checks validate the selected checkout against the pin and
declared patch manifest. The new test-only dependency is `websockets==15.0.1`;
production HTTP and sockets use Godot's native transports.

## Completed gates

| Gate | Result |
|---|---|
| Pinned dev editor/module build, tests and warnings as errors | Passed |
| Module contracts | 114 cases, 13,578 assertions passed |
| Godot TextEdit/RichTextLabel regressions | 15 cases, 4,232 assertions passed |
| Python script/fixture contracts | 26 tests passed |
| JavaScript contracts, after `npm ci` in both packages | 32 tests passed per package |
| Complete headless suite | All 19 discovered manifests passed |
| Native Linux display | Gallery, presentations and theme/geometry passed under Xvfb/software OpenGL |
| Formatting, tracked smoke inputs and engine provenance | Passed |
| Standalone staged project | Imported and ran all four application roots successfully |

The headless suite retains all eleven existing renderer/component scenarios and
adds `game-direct-sync`, `game-frame-sync`, `game-root-lifecycle`,
`game-inventory-ui`, `game-http-binary`, `game-websocket-sync`,
`game-theme-geometry`, and `game-presentations`. Each manifest runs once in the
common runner. Success markers follow native identity/value/geometry checks,
actual React observations and, for network scenarios, local server records.

Contracts cover monotonic scheduling, a shared native/timer/idle callback budget,
per-callback Hermes checkpoints and enqueue-ordered native events/completions;
typed Dictionary schemas/defaults and unrelated script capabilities; session and
surface isolation; native inherited/capped/disabled font scaling; settings bounds;
Blob chunks exceeding the codec ceiling, collector reclamation and URL/native
pins; cookie scope/expiry/bounds and cross-registrant isolation; and event queue
saturation across sockets, scene bindings, application state and custom Alerts.

Review regressions additionally verify that continuously due intervals rotate
behind waiting timers/rAF, relative redirects preserve paths/queries/fragments,
HTTP body limits fit the aggregate buffer budget, full event queues retry socket
open before messages, and Keyboard-only consumers instantiate the native service.
Immediate native HTTP start failures leave no owned request tokens after twenty
repeated attempts, and drained leases release their buffer reservations. Enemy death
emits once per positive-to-zero health transition; repeated damage and a later life
cannot call a freed panel, and sibling surfaces remain valid.
Further review regressions exercise metadata/data listener failures and successful
Blob handoff followed by a throwing completion listener, prototype-colliding HTTP
header names, invalid secure cookie prefix replacements/deletions, and sync/queued
Object command results with session/capability/destruction checks. The unrelated
script fixture checks nested/nullable Object arguments, results and signal payloads,
and exact `int64` wrappers within and above the JavaScript safe-integer range in both modes.
Typed Blob URLs retain response header/Blob MIME metadata after source close.
WebSocket Blob quota exhaustion emits one error/abnormal close, preserves byte accounting
and releases the peer; adapter tests also cover failed append/finish and sibling events.
The actual RN application executes the tagged bridgeless immediate shim through native
Hermes microtasks, verifying cancellation, arguments and nested delivery.
Explicit zero idle timeouts run even with no spare frame time, and an empty coordinator
disconnects frame polling after pending delivery before reconnecting on root entry.
Borrowed HTTP/multipart uploads pin backing before enqueue; unit tests cover conversion,
submission, callback, cancellation and asynchronous failures, while actual fetch requests
close their source Blobs before native copying. Appearance null clears the override in
both adapter and application/display tests. Ordinary Secure cookies reject insecure
replacement, deletion and child-path overlays without blocking unrelated or expired entries.
Nullable primitive/collection schema attachments reject typed script parameters while
Variant commands accept null and nonnull values. Required arguments cannot follow
optional/defaulted arguments, and valid trailing script defaults retain required validation.
Mixed-case HTTP duplicate fields merge under their first spelling, including prototype
names. Oversized non-expiring cookie replacements preserve valid stored entries while
explicit expiration still deletes them.
Aggregate completion payloads are limited before copying; native tests verify rejection
and accounting recovery on cancellation, delivery and reset. Idle frame polling stops
with passive application-service listeners, Alert subscriptions follow root lifetime,
and a tree-owned observer tracks pause/resume while no RN roots are mounted. The HTTP
fixture verifies tagged BlobRegistry sibling retention and last-close reclamation for
shared Blob slices, alongside independent Blob copies; collector-only reclamation retains its distinct sibling-lifetime contract.
The HTTP fixture also exercises query-only and root-relative redirects through
the actual HTTPRequest/fetch path.

The application suite checks root removal/re-entry/reload and generation cleanup,
direct inventory commands, lists/editors/assets, application metrics and locale
RTL invalidation, native pause/resume, same-root/custom Alert and Godot bubbling.
Real HTTPRequest scenarios include binary/NUL, non-2xx, gzip, verified fixture TLS,
redirects/cookies, multipart, cancellation/timeout, shared image cache/credential
isolation, pool reuse and native backing reclamation. WebSocketPeer scenarios
include text/binary/protocol, ordered backpressure, message limits, graceful and
unclean close, and reset cleanup before Godot-authoritative UI updates.

Local logs are under `artifacts/phase6b-*.log` and `artifacts/smoke-logs/`; CI
uploads smoke/import/fixture diagnostics on failure. Reproduce from the binding
repository after bootstrap:

```sh
export GODOT_SOURCE_DIR="$PWD/.phase6a/godot" # or the clean pinned default checkout
scripts/verify_provenance.py
scripts/check_format.sh
python3 -m unittest discover -s scripts/tests
npm --prefix samples/view-text ci
npm --prefix samples/view-text run test:godot
npm --prefix samples/game-ui ci
npm --prefix samples/game-ui run test:godot
scripts/build_godot.sh -j2 accesskit=no module_mono_enabled=no
scripts/run_cpp_tests.sh
"$GODOT_SOURCE_DIR/bin/godot.linuxbsd.editor.dev.x86_64" --headless --test --test-case='*[TextEdit]*,*[RichTextLabel]*'
python3 scripts/run_smoke_tests.py
scripts/run_component_display.sh
python3 scripts/stage_game_demo.py
"$GODOT_SOURCE_DIR/bin/godot.linuxbsd.editor.dev.x86_64" --headless --editor --path ../godotProjects/Phase6BGame --import
```

Install `requirements-ci.txt` in the Python environment used by the runner, or set
`SMOKE_FIXTURE_PYTHON` to that environment's interpreter. The display gate requires
Xvfb, xauth and software OpenGL, or `COMPONENT_DISPLAY_READY=1` on an actual display.
Use clang-format 18, matching Ubuntu 24.04 CI; newer versions format constructor
initializers differently. The initial CI formatting failure was corrected with
clang-format 18.1.8 and its full formatting gate passed locally.
See [demo instructions](../samples/game-ui/DEMO.md) for interactive play.

The review regressions also verify replacement UTF-8 for lone UTF-16 surrogates in
Blob/HTTP/WebSocket strings, a 2 MiB HTTP download within a 1.5-second deadline at
5 FPS, and noncancelable native alert recovery after window close. Native services
retain bounded resynchronization/state notifications or reject unavailable custom
presentation explicitly. Cookies are host-only; Domain attributes are rejected.
Collector regressions retain a live sibling, attach a replacement before draining a
release, and retain backing until the last native pin drops. XHR Blob responses preserve
MIME metadata from mixed-case Content-Type headers. Alert/session types match the native
dismissal result and opaque handle wrapper.

## Display and input-method boundary

Xvfb verifies native focus, selection/editing, clipping, popup/window lifetime,
root input ordering, Alert presentation and clipboard round-trip. This host has
no XIM input-method server; it emits the exact missing-XIM warning in the display
allowlist. Native composition notification/replacement/rollback contracts pass,
but no OS IME candidate-window result is claimed.

To reproduce the remaining real Linux input-method check:

1. Start an actual Linux desktop with an active IBus or Fcitx input method and
   launch the pinned editor from that session, retaining its normal input-method
   environment.
2. Build the gallery and open `samples/game-ui` with
   `res://smoke/tests/component_contracts/SmokeMain.tscn`, or open the staged game.
3. Focus the uncontrolled editor, compose non-Latin text, inspect candidate-window
   placement, commit/cancel composition and verify one insertion. Repeat in the
   controlled editor and the native Window editor.
4. During composition, change the controlled value and reload/remove the owning
   surface; check native selection/focus and absence of stale callbacks or duplicate
   insertion. Record desktop/backend/input-method versions and observed results.

Result on this host: actual candidate-window behavior remains unverified because
only Xvfb is available. Broader backend IME conformance remains pending.

## Declared limits and compatibility review

HTTP exposes complete responses, with synthesized XHR state progression rather
than incremental responseText. Appearance is application-controlled unless host
following is opted in. SafeAreaView retains the upstream non-iOS View fallback;
RefreshControl remains pending and the game uses an explicit refresh button.
Unsupported Clipboard/Linking/Vibration providers reject explicitly. Headless
Window evidence covers logical content/lifetime, with actual native presentation
checked separately on the display lane.

Full animation/transform/rich-text fidelity, accessibility/AccessKit, screenshot
diffs, handheld behavior, other export targets, 3D and complete upstream renderer
adoption remain outside this gate. Public RN compatibility rows remain partial or
pending where those differences or missing evidence apply.

The bounded compatibility review inspected tagged `v0.87.1` public exports,
Flow/types, FabricUIManager, style contracts and the relevant Alert, timers,
networking/Blob/WebSocket, AppRegistry, appearance/metrics/lifecycle, I18n,
Keyboard, Clipboard, Linking, Vibration and emitter implementations. Public row
IDs and existing single-platform exclusions were retained; no public surface was
added or removed. Service/component/style evidence and application/test rows were
updated. Godot-only helpers and internal hooks remain in extension documentation,
outside the RN public matrices.
