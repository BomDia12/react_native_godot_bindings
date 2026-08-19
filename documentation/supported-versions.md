# Supported baseline versions

`baseline.env` is the machine-readable source of truth. Source and runtime entries are
exact pins; toolchain entries describe the reproducible Linux CI environment.

| Component | Support | Enforcement |
|---|---|---|
| Godot | Exact commit `2c089e9bf0b8712d0bc444c2ceaf9c543ed9c777` | `bootstrap.sh`, provenance validation |
| Hermes | Exact commit `fd0e1d3ed928510b30fd74b792a826f9d0457064` (`hermes-v0.15.0`) | Git submodule, build script |
| React Native | `0.84.1` | Package manifest and lockfile |
| React | `19.2.3` | Package manifest and lockfile |
| Metro | `0.83.5` | Direct dependency and lockfile |
| Yoga | React Native commit `6747cd2a6c137a2f3317082b51ac8e30b878b00d` | Vendored-tree digest |
| Node.js | CI `22.11.0`; local `>=22.11.0` | `.nvmrc`, package engines |
| Python | CI `3.12` | Workflow |
| SCons | CI `4.8.1` | `requirements-ci.txt` |
| CMake | CI `3.28` | Workflow |
| Ninja | CI `1.11` | Workflow |
| Compiler | CI GCC 13 with C++20 | Workflow and Godot build |

Only the Linux editor and headless runtime path are currently tested. Other platforms
remain pending in the compatibility matrix.
