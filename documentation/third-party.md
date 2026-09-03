# Third-party provenance

## Hermes

- Repository: `https://github.com/facebook/hermes.git`
- Revision: `3477757eb2475555cf8d8df24bfb1deb0613880d`
  (`hermes-v250829098.0.17`)
- Local destination: `modules/react_native_bindings/engines/hermes`
- Acquisition: pinned Git submodule
- License: MIT at `modules/react_native_bindings/engines/hermes/LICENSE`
- Modifications: none
- Build: `scripts/build_hermes.sh`, Release shared `hermesvm` and `jsi`, Hades GC,
  tests disabled
- Update: change the submodule revision and `baseline.env` together, then rebuild from
  an empty `engines/build_release` directory

## Yoga

- Repository: `https://github.com/facebook/react-native.git`
- Revision: `a59eff64fa907ed6e919fafe6cbd26d1d54c2de3` (React Native `v0.87.1`)
- Upstream path: `packages/react-native/ReactCommon/yoga/yoga`
- Local destination: `modules/react_native_bindings/thirdparty/yoga/yoga`
- Acquisition: vendored source copy
- License: MIT at `modules/react_native_bindings/thirdparty/yoga/LICENSE`
- Modifications: none
- Build: explicit source selection in the module `SCsub`, using a cloned SCons
  environment with vendored warnings disabled
- Update: follow `modules/react_native_bindings/thirdparty/yoga/UPSTREAM.md`

Godot, React Native, React, Metro, Node.js, and the compiler toolchain are pinned build or
runtime inputs rather than vendored module source. Their versions are recorded in
`baseline.env` and `documentation/supported-versions.md`.
