# Component validation

The Phase 6A-owned component gate passed locally on the pinned Linux dev editor.
CI runs the same unit, smoke and Xvfb display checks.

The agreed gate covers **6A-owned components**. Phase 6B is absent from this
checkout. Its real HTTP/WebSocket transport, generation scheduler, Keyboard,
Alert presenter, scene bindings and combined gameplay integration are deferred.
Contract fixtures can verify those interfaces; they do not establish integrated
support. No substitute networking, scheduler or scene-binding service is part of
this change.

## Engine provenance

The Godot pin remains in `baseline.env`. Additive native contracts are tracked in
`patches/godot/manifest.json`; each patch has a SHA-256 digest. Bootstrap and build
apply the exact patch set idempotently. Provenance validation checks the resulting
source against the pinned originals plus declared patches. Unrelated engine edits
are rejected before any file is changed. Git worktrees are supported.

CI caches clean pinned source, applies the declared contracts after cache restore,
and includes the patch set in compiler cache identity.

## Evidence

| Gate | Result |
|---|---|
| Pinned dev editor, tests enabled, warnings treated as errors | Passed |
| Module unit tests | 73 cases, 805 assertions passed |
| Godot TextEdit/RichTextLabel default-behavior regression tests | 15 cases, 4,232 assertions passed |
| Python script contracts | 22 tests passed |
| JavaScript contracts, each sample package | 15 tests passed, including real Metro development/production resolution |
| Complete headless smoke gate | All 11 scenarios passed, including the ten renderer regressions |
| Native Linux display gate | Component scenario passed under Xvfb, software OpenGL and native Window input |
| Formatting and provenance | Passed; exact engine patch identity verified |

The component scenario asserts imported/base64 image rendering and getSize, stable
native editing across rerenders and multiline replacement, controlled input, native
list offsets/virtualization/viewability, sticky section headers, Switch updates and inherited pointer blocking,
nested Modal dismissal, stable context-menu IDs, secondary-button isolation,
Button/Pressable activation, authored CanvasLayer and overlapping-root input blocking, native Window
editing and a Modal using that window's viewport. Three bare enemy roots retain
independent presentation state; the fixture does not provide scene authority.

Unit contracts additionally cover publication rollback, authored/internal child
containers, resource remeasurement, native shaping/selection and constrained sizing,
image allocation/credential/cancellation ownership, editor acknowledgment ordering,
native undo/redo and IME replay, scroll viewport/anchor/offset precedence, and nested
modal focus ownership. Default native editor/text regression tests exercise the
engine APIs with the optional contracts unused.

Run from the binding repository after bootstrap and build:

```sh
scripts/check_format.sh
python3 -m unittest discover -s scripts/tests
npm --prefix samples/view-text run test:godot
npm --prefix samples/game-ui run test:godot
godot/bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case='*[ReactNativeBindings]*'
godot/bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case='*[TextEdit]*,*[RichTextLabel]*'
scripts/run_baseline.sh
scripts/run_component_display.sh
```

For a clean detached Godot worktree, set `GODOT_SOURCE_DIR` for build, provenance
and smoke/display scripts. Runtime/import logs are under `artifacts/smoke-logs/`.
The display-only allowlist accepts the two exact Xvfb diagnostics for missing XIM
and GLX swap-interval support; it does not relax headless diagnostics. OS IME
candidate-window behavior, screenshot diffs, sanitizer coverage and other export
targets remain unverified.

## Compatibility review

Reviewed the RN `v0.87.1` public `index.js`, `index.js.flow`,
`Libraries/ReactNative/FabricUIManager.js`, `Libraries/StyleSheet/StyleSheetTypes.js`
and `.d.ts`, and tagged implementations/types for ScrollView, TextInput, Modal,
ActivityIndicator, Switch, Image, Button and TouchableOpacity. Exact source
adaptations also verify the installed TextInputState and Animated props-hook digests.

Public row IDs and platform exclusions are preserved; no public surface rows were
added or removed. Native component/style claims are partial, with explicit Godot
adaptations and evidence. General Animated, Keyboard, Alert and networking remain
pending. The test matrix adds the bounded descriptor/component/display gates and
updates command/platform evidence. No new single-platform RN surface is classified.
Unverified full RN behavior remains partial or pending rather than inferred from
component mounting.

## Native adaptations

Text uses Godot RichTextLabel, TextParagraph and TextServer. Font sizes and glyph
spacing use native integer rounding. The current ellipsis implementation uses
native tail trimming. Deterministic fonts belong to test fixtures; application
defaults follow Godot themes. Fonts and StyleBoxes are not modified in place.

View shadows use one Godot outset StyleBox shadow. Blur and spread map to its
integral shadow radius. Multiple/inset shadows, nonsolid borders, 3D transforms
and perspective report unsupported values. Sibling zIndex changes only renderer
children within their existing native container; DOM traversal remains logical.

Desktop middle/right click events observe button press, matching native context
menu activation. They never produce a primary touch or Pressable activation.
Context menus use native PopupMenu and stable application-supplied IDs.
Accessibility metadata and declared actions are retained; full AccessKit
conformance remains a later milestone.
