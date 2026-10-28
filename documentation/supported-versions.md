# Supported baseline versions

`baseline.env` is the machine-readable source of truth. Source and runtime entries are
exact pins; toolchain entries describe the reproducible Linux CI environment.

| Component | Support | Enforcement |
|---|---|---|
| Godot | `4.7.2`, exact commit `ed1daf0bf001b61586d9930840f2f1394092c079`, unmodified | `bootstrap.sh`, provenance validation |
| Hermes | Exact commit `3477757eb2475555cf8d8df24bfb1deb0613880d` (`hermes-v250829098.0.17`) | Git submodule, build script |
| React Native | `0.87.1` | Package manifest and lockfile |
| React | `19.2.3` | Package manifest and lockfile |
| Metro | `0.87.2`, through React Native's `^0.87.0` range | `react-native-godot` dependency and lockfiles |
| React Native Community CLI | `20.2.0`, the version React Native 0.87.1's own tooling uses | Template and sample manifests |
| React Native core | `v0.87.1` commit `a59eff64fa907ed6e919fafe6cbd26d1d54c2de3`, including Yoga and JSI | Git submodule, provenance validation |
| folly, glog, fmt, double-conversion, fast_float, nlohmann/json | React Native 0.87.1 pins | Git submodules, provenance validation |
| Boost | `1.83.0` headers | `bootstrap.sh` SHA-256 check |
| AccessKit C SDK | `0.22.3`, selected by the pinned Godot commit | `bootstrap.sh` through Godot's `install_accesskit.py` |
| Mesa | CI Ubuntu 24.04 `libgl1-mesa-dri`, llvmpipe under Xvfb, for the visual gate. The committed goldens were rendered with Mesa 26.2.5 llvmpipe; CI compares them within tolerance | Visual regression goldens |
| Node.js | CI `22.13.0`; local `>=22.13.0` | `.nvmrc`, package engines |
| Python | CI `3.12` | Workflow |
| SCons | CI `4.8.1` | `requirements-ci.txt` |
| websockets | Test-only `15.0.1` | `requirements-ci.txt`, localhost fixture |
| CMake | CI `3.28` | Workflow |
| Ninja | CI `1.11` | Workflow |
| Compiler | CI GCC 13 with C++20 | Workflow and Godot build |
| C++ formatter | Clang Format 18 | `check_format.sh`, Ubuntu CI toolchain |

Each prerelease `react-native-godot.tgz` contains a `godot-platform.json` written by
`scripts/package_npm.sh`. It records the repository commit, the Godot commit, the React
Native version and the URL and SHA-256 of the editor archive built from the same commit,
which the CLI downloads and verifies. That archive is the release's
`react-native-godot-linux-x86_64-<commit>.zip` asset, which later prereleases keep. Its version is `0.1.0-preview.g<commit>`.

Only the Linux editor and headless runtime path are currently tested. The Godot Metro
adapter map is pinned to the React Native 0.87.1 public module layout and must be reviewed
with every React Native update. Other build/export targets remain pending in the
compatibility matrix.

The module builds against the unmodified pinned engine. Behavior that stock controls
lack comes from module subclasses of the engine classes. Bootstrap refuses a Godot
checkout with local changes, and provenance validation fails on any engine edit.
Native dependencies require advanced TextServer, JPEG and WebP; PNG support is provided
by Godot core. HTTP/TLS/WebSocket require core HTTPRequest plus the WebSocket and mbedTLS
modules.

The Linux component display gate uses Xvfb and software OpenGL in CI. Automated
IME notification replay verifies composition ownership and rollback; platform IME
candidate presentation and other export targets remain unverified.

The display gate uses its own exact warning allowlist for Xvfb's missing XIM server
and GLX swap-interval extension. Headless diagnostics retain the baseline allowlist;
other native warnings or errors fail either gate.
