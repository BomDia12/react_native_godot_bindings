# React Native bindings for Godot

This repository builds the React Native 0.87.1 component layer as an
external Godot module. Godot is cloned into the ignored `godot/` working directory; the
tracked module remains under `modules/react_native_bindings/`.

The tested Linux path includes the Godot Metro platform, explicit RGBA colors, staged
local assets, C++ host descriptors and native modules, `Pressable` interaction,
independent React surfaces, public host refs, retained transactional mounting, native
images, scrolling, editing, Modal, Switch and GodotWindow presentation. The
[component gallery](samples/game-ui/) exercises upstream FlatList and SectionList.
Real network, scheduler, Keyboard, Alert and scene-binding integration awaits Phase 6B. See
the [Godot platform](documentation/godot-platform.md),
[extension](documentation/extensions.md), [interop](documentation/interop.md),
[compatibility matrices](documentation/compatibility/), and
[test coverage](documentation/compatibility/test-coverage.md) for the implemented
boundary and remaining limitations.

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
