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

## Godot native contracts and sample font

Godot remains pinned to `ed1daf0bf001b61586d9930840f2f1394092c079`.
`patches/godot/manifest.json` records the exact SHA-256 patch set. The patches extend
native controls/text/editor checkpoints, add image bounds/probes and optional
scoped allocator hooks to bundled libwebp. Default APIs retain native behavior.
Godot's MIT license and the existing libwebp BSD license remain in the pinned
checkout. The WebP allocation hook is enabled only for the bundled codec.

`samples/game-ui/assets/OpenSans.woff2` copies the pinned Godot
`thirdparty/fonts/OpenSans_SemiBold.woff2` unchanged. Its SIL Open Font License 1.1 is
included as `assets/LICENSE.OpenSans.txt`. This is a deterministic sample font;
applications inherit their Godot theme by default.

React Native files are not vendored or modified in references. Narrow Metro source
adaptations are guarded by SHA-256 of the installed 0.87.1 source bytes in
`js/godot/source-adaptations.json`, under the upstream React Native MIT license.

## Local network fixture

`requirements-ci.txt` pins test-only `websockets==15.0.1` (BSD-3-Clause, upstream
https://github.com/python-websockets/websockets). It runs only in the Python localhost
fixture and is not linked or shipped in the Godot runtime. HTTP/TLS fixtures use Python
stdlib servers; the checked-in test certificate/key identifies only localhost, is
non-production test material, and is trusted explicitly in the test project. Native
HTTP/TLS/WebSocket use Godot's existing facilities and licenses.
