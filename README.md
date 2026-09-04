# React Native bindings for Godot

This repository builds the current React Native 0.87.1 `View`/`Text` baseline as an
external Godot module. Godot is cloned into the ignored `godot/` working directory; the
tracked module remains under `modules/react_native_bindings/`.

## Build and test

Install Git, Node.js 22.11 or newer, Python, SCons, CMake, Ninja, and a C++20 compiler,
then run:

```sh
git clone --recurse-submodules https://github.com/BomDia12/react_native_godot_bindings.git
cd react_native_godot_bindings
scripts/bootstrap.sh
scripts/build_hermes.sh
scripts/build_godot.sh
scripts/run_baseline.sh
```

`bootstrap.sh` creates or verifies the pinned `godot/` checkout. `build_hermes.sh` builds
the pinned Hermes submodule from source. `build_godot.sh` attaches `modules/` through
Godot's `custom_modules` option. `run_baseline.sh` discovers the restricted manifests at
`samples/*/smoke/tests/*/smoke_test.json`, creates fresh Metro bundles, and runs every
declared headless smoke test. Shared manifests with the same package, npm script, and
output use one dependency install and one bundle build. Set `SMOKE_JOBS` to change the
default concurrency of two Godot processes.

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
