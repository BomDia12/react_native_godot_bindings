# React Native bindings for Godot

This repository builds the current React Native 0.84.1 `View`/`Text` baseline as an
external Godot module. Godot is cloned into the ignored `godot/` working directory; the
tracked module remains under `modules/react_native_bindings/`.

## Build and test

Install Git, Node.js 22.11 or newer, Python, SCons, CMake, Ninja, and a C++20 compiler,
then run:

```sh
git clone --recurse-submodules https://github.com/BomDia12/react-native-godot-bindings.git
cd react-native-godot-bindings
scripts/bootstrap.sh
scripts/build_hermes.sh
scripts/build_godot.sh
scripts/run_baseline.sh
```

`bootstrap.sh` creates or verifies the pinned `godot/` checkout. `build_hermes.sh` builds
the pinned Hermes submodule from source. `build_godot.sh` attaches `modules/` through
Godot's `custom_modules` option. `run_baseline.sh` validates provenance and compatibility,
creates a fresh Metro bundle, and runs the semantic headless smoke test.

Use `GODOT_SOURCE_DIR=/path/to/godot` to build against another checkout of the pinned
commit. Extra arguments passed to `build_godot.sh` are forwarded to SCons, for example:

```sh
scripts/build_godot.sh -j8 cpp_compiler_launcher=ccache
```

Supported versions are defined in `baseline.env` and explained in
`documentation/supported-versions.md`.
