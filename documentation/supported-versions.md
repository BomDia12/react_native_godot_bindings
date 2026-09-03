# Supported baseline versions

`baseline.env` is the machine-readable source of truth. Source and runtime entries are
exact pins; toolchain entries describe the reproducible Linux CI environment.

| Component | Support | Enforcement |
|---|---|---|
| Godot | `4.7.2`, exact commit `ed1daf0bf001b61586d9930840f2f1394092c079` | `bootstrap.sh`, provenance validation |
| Hermes | Exact commit `3477757eb2475555cf8d8df24bfb1deb0613880d` (`hermes-v250829098.0.17`) | Git submodule, build script |
| React Native | `0.87.1` | Package manifest and lockfile |
| React | `19.2.3` | Package manifest and lockfile |
| Metro | `0.87.0` | Direct dependency and lockfile |
| Yoga | React Native `v0.87.1` commit `a59eff64fa907ed6e919fafe6cbd26d1d54c2de3` | Vendored-tree digest |
| Node.js | CI `22.13.0`; local `>=22.13.0` | `.nvmrc`, package engines |
| Python | CI `3.12` | Workflow |
| SCons | CI `4.8.1` | `requirements-ci.txt` |
| CMake | CI `3.28` | Workflow |
| Ninja | CI `1.11` | Workflow |
| Compiler | CI GCC 13 with C++20 | Workflow and Godot build |

Only the Linux editor and headless runtime path are currently tested. Other platforms
remain pending in the compatibility matrix.
