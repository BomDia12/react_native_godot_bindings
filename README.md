# React Native bindings for Godot

This repository builds the React Native 0.87.1 component layer as an
external Godot module. Godot is cloned into the ignored `godot/` working directory; the
tracked module remains under `modules/react_native_bindings/`.

The tested Linux path includes independent React surfaces, retained native controls,
images, lists, editing and presentations, a monotonic frame scheduler, pooled Godot
HTTPRequest, WebSocketPeer and chunked Blob services. The [working game](samples/game-ui/DEMO.md)
keeps enemy and inventory state in GDScript: signals update four RN surfaces directly,
and declared RN commands mutate only their bound Godot target. Network scenarios feed
updates into the same Godot authority.

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
npm --prefix samples/view-text ci
npm --prefix samples/view-text run test:godot
scripts/run_baseline.sh
scripts/run_component_display.sh
```

`bootstrap.sh` creates or verifies the pinned `godot/` checkout. `build_hermes.sh` builds
the pinned Hermes submodule from source. `build_godot.sh` attaches `modules/` through
Godot's `custom_modules` option. `run_baseline.sh` discovers the restricted manifests at
`samples/*/smoke/tests/*/smoke_test.json`, creates fresh Metro bundles, and runs every
declared headless smoke test. Shared manifests with the same package, npm script, and
output use one dependency install and one bundle build. Set `SMOKE_JOBS` to change the
default concurrency of two Godot processes.

The sample's `build:godot` script uses the repository-owned Metro config and stages its
bundle, source map, dependency evidence, and local asset variants under
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
the default. All roots share one Hermes runtime and one evaluated bundle, while each root
owns an independent React surface.

Calling `reload()` or changing a live root's application key restarts only that surface
and assigns it a new root tag. A React Native file refresh is process-wide: it resets
Hermes once, evaluates the shared bundle once, and restarts every live root with a new
tag.

Successful merges to `main` replace the `baseline-linux-latest` rolling prerelease. Its
Linux archive contains the Godot editor executable, `libhermesvm.so`, and `libjsi.so`,
with a separate SHA-256 checksum asset.
