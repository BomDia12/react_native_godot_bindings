# Third-party provenance

## Hermes

- Repository: `https://github.com/facebook/hermes.git`
- Revision: `fd0e1d3ed928510b30fd74b792a826f9d0457064` (`hermes-v0.15.0`)
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
- Revision: `6747cd2a6c137a2f3317082b51ac8e30b878b00d`
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
