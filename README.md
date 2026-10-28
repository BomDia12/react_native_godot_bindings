# React Native bindings for Godot

This repository runs React Native 0.87.1 inside Godot as an external module. It builds
upstream React Native's C++ core (Fabric, the TurboModule system and `ReactHost`) from
pinned submodules and adds a Godot platform: component views over Godot controls,
TurboModules over Godot services, and the `react-native-godot` JavaScript package. Godot is cloned into the ignored `godot/` working directory; the
tracked module remains under `modules/react_native_bindings/`.

The tested Linux path includes independent React surfaces, retained native controls,
images, lists, editing and presentations, a JavaScript thread separate from Godot's main thread, pooled Godot
HTTPRequest, WebSocketPeer and chunked Blob services. The [working game](samples/game-ui/DEMO.md)
keeps enemy and inventory state in GDScript: signals update four RN surfaces directly,
and declared RN commands mutate only their bound Godot target. Network scenarios feed
updates into the same Godot authority.

## Use it in a Godot project

Add React Native to a Godot project, new or existing, with the package and editor from the
rolling Linux prerelease. Node.js 22.13.0 or newer is required:

```sh
cd my-godot-game
npx --yes --package=https://github.com/BomDia12/react_native_godot_bindings/releases/download/baseline-linux-latest/react-native-godot.tgz react-native-godot-init
cd react_native
npx react-native run-godot --editor
```

See [getting started](documentation/getting-started.md) for what this creates and how to
build, run and debug it.

## Demo game

Build and stage the standalone demo under the workspace's `godotProjects` directory:

```sh
python3 -m pip install -r requirements-ci.txt
npm --prefix samples/game-ui ci
npm --prefix samples/game-ui run build:godot
python3 scripts/stage_game_demo.py
"${GODOT_SOURCE_DIR:-$PWD/godot}/bin/godot.linuxbsd.editor.dev.x86_64" --editor --path ../godotProjects/Phase6BGame --import
"${GODOT_SOURCE_DIR:-$PWD/godot}/bin/godot.linuxbsd.editor.dev.x86_64" --path ../godotProjects/Phase6BGame
```

Play offline with **1/2/3** to damage enemies and **P** to pick up a potion. An optional
HTTP endpoint and explicit Refresh button demonstrate remote inventory/icons. Service
limits use ordinary `react_native/*` Project Settings. Prefer uncontrolled TextInput
`defaultValue` for native editing; controlled `value` with `onChangeText` is retained.
The [platform](documentation/godot-platform.md), [extensions](documentation/extensions.md),
[interop](documentation/interop.md), [compatibility matrices](documentation/compatibility/)
and [validation](documentation/phase6b_validation.md) describe the tested boundary.

## Build and test

Install Git, Node.js 22.13.0 or newer, Python, SCons, CMake, Ninja, and a C++20 compiler,
then run:

```sh
git clone --recurse-submodules https://github.com/BomDia12/react_native_godot_bindings.git
cd react_native_godot_bindings
scripts/bootstrap.sh
scripts/build_hermes.sh
scripts/build_godot.sh
npm --prefix packages/react-native-godot ci
node packages/react-native-godot/scripts/codegen.cjs
node packages/react-native-godot/scripts/check-overrides.cjs
npm --prefix packages/react-native-godot test
npm --prefix samples/view-text ci
scripts/run_baseline.sh
scripts/run_component_display.sh
scripts/run_visual_regression.sh
scripts/run_staged_game.sh
scripts/run_consumer.sh
```

`bootstrap.sh` creates or verifies the pinned `godot/` checkout, initializes the pinned
React Native, folly, glog, fmt, double-conversion, fast_float and nlohmann/json submodules,
downloads the Boost headers, and installs the AccessKit SDK that the pinned engine's
`misc/scripts/install_accesskit.py` selects, reinstalling it when that version changes. `build_hermes.sh` builds the pinned Hermes submodule
against React Native's JSI. `codegen.cjs` regenerates the committed React Native codegen
output in `modules/react_native_bindings/generated/`. `build_godot.sh` attaches `modules/` through
Godot's `custom_modules` option. `run_baseline.sh` discovers the restricted manifests at
`samples/*/smoke/tests/*/smoke_test.json`, creates fresh Metro bundles, and runs every
declared headless smoke test. Shared manifests with the same package, npm script, and
output use one dependency install and one bundle build. Set `SMOKE_JOBS` to change the
default concurrency of two Godot processes. `run_staged_game.sh` stages the game demo
built by the smoke run into a temporary project outside the repository, imports it, and
checks that all four of its roots mount without diagnostics. `run_consumer.sh` packs
`react-native-godot`, creates a consumer project from it with `react-native-godot-init`, and
runs that project from a bundle file and from Metro.
`run_visual_regression.sh` builds the fidelity bundle, imports the game sample, and renders
each `samples/game-ui/smoke/visual/*.tscn` application under Xvfb with software OpenGL,
comparing it with its golden PNG in `smoke/visual/goldens/`. Failing captures are written
to `artifacts/visual/`; `RN_VISUAL_UPDATE=1` rewrites the goldens instead. Like
`run_component_display.sh`, `COMPONENT_DISPLAY_READY=1` runs on the current display
without Xvfb.

The JavaScript platform is the `react-native-godot` package in `packages/react-native-godot/`.
Samples depend on it through `file:../../packages/react-native-godot` and use it as a
consumer would: the sample's `build:godot` script runs `react-native build-godot`, which
stages its bundle, source map, dependency evidence, and local asset variants under
`samples/view-text/dist/`. Application resolution prefers `.godot.*`, then `.native.*`,
then generic files; the supported graph does not select Android/iOS platform files.

Use `GODOT_SOURCE_DIR=/path/to/godot` to build against another checkout of the pinned
commit. Extra arguments passed to `build_godot.sh` are forwarded to SCons, for example:

```sh
scripts/build_godot.sh -j8 cpp_compiler_launcher=ccache
```

Supported versions are defined in `baseline.env` and explained in
`documentation/supported-versions.md`.

## Runtime and surface setup

Build one Metro bundle that registers every application key used by the Godot scene. Set
each `ReactNativeRootView.application_key` to one of those registered keys; `GodotApp` is
the default. All roots share one `ReactNativeHost`, one Hermes runtime and one evaluated
bundle, while each root owns an independent React surface. JavaScript runs on its own
thread; `await ReactNativeHost.evaluate(code)` evaluates code from GDScript.

Calling `reload()` or changing a live root's application key restarts only that surface
and assigns it a new root tag. A React Native file refresh is process-wide: it recreates
the host once, evaluates the shared bundle once, and restarts every live root with a new
tag.

## Developer server

Turn on `react_native/dev_server/enabled` in Project Settings and start Metro in the
JavaScript project:

```sh
npm --prefix samples/view-text run start:godot
```

`start:godot` runs the package's `react-native-godot-start`, which serves the project's
`metro.config.js` with React Native's dev middleware; `npx react-native run-godot` starts it
for you. `react_native/dev_server/host`
and `react_native/dev_server/port` (default `localhost:8081`) locate Metro, and
`react_native/dev_server/entry` (default `index`) names the entry file without its
extension, for example `godot.entry` in `samples/view-text`. The runtime downloads its bundle from Metro and falls back to
`react_native/bundle/path` when Metro can't serve it. Exported games always load the
bundle file.

Saved edits apply through Fast Refresh, and errors open LogBox above the game. Press **r**
in Metro, reload from DevTools or call `DevSettings.reload()` to reload every root through
`ReactNativeHost.reload()`. Press **j** in Metro or call `ReactNativeHost.open_debugger()`
to open React Native DevTools.

Successful merges to `main` replace the `baseline-linux-latest` rolling prerelease. Its
Linux archive contains the Godot editor executable, `libhermesvm.so`, and `libjsi.so`,
with a separate SHA-256 checksum asset.
