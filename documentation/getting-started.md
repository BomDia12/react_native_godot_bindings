# Getting started

React Native for Godot runs on a Godot editor built with this repository's module. The
rolling Linux prerelease ships that editor and the `react-native-godot` package, so a project
needs no compiler. Linux x86_64 and Node.js 22.13.0 or newer are required.

## Create a project

From a Godot project directory, or an empty directory for a new project:

```sh
npx --yes --package=https://github.com/BomDia12/react_native_godot_bindings/releases/download/baseline-linux-latest/react-native-godot.tgz react-native-godot-init
```

`react-native-godot-init` accepts `--path <godot project>`, `--root <folder>` for the
JavaScript project (default `react_native`) and `--godot <editor>`. It:

1. creates `project.godot` when the directory has none;
2. writes the JavaScript project from the template, refusing to overwrite existing files;
3. copies the package into `react_native/vendor/` and depends on it from there, so the
   lockfile and the editor download stay valid when the prerelease is replaced;
4. runs `npm install` and writes `node_modules/.gdignore`;
5. adds the `[react_native]` settings that the project doesn't set yet;
6. adds `Main.tscn`, a full-window `ReactNativeRootView` showing the `App` component, when
   the project has no main scene;
7. downloads the matching editor once to `~/.cache/react-native-godot/`.

```text
res://
  project.godot
  Main.tscn
  react_native/
    package.json  index.js  register.js  App.tsx  tsconfig.json  metro.config.js  babel.config.js
    godot/           TypeScript generated from component contracts
    vendor/react-native-godot-<version>.tgz
    dist/            bundle, source map and assets, after a build
    node_modules/
```

```ini
[react_native]
project/root="res://react_native/"
bundle/path="res://react_native/dist/index.bundle.js"
dev_server/enabled=true
dev_server/entry="index"
```

`register.js` registers components with `GodotAppRegistry.registerComponent`, and
`index.js` imports it. Neither needs a setup import: the package's Metro configuration runs
`react-native-godot/setup` first. `index.js` then imports `godot/registry`, which registers
the components that have [component contracts](component-contracts.md): typed properties
and events that Godot sets and receives on the root. It registers last, so a contract for a
key that `register.js` also registers, such as `App`, keeps its contract.

## Run

From `react_native/`:

```sh
npx react-native run-godot --editor  # open the Godot editor
npx react-native run-godot           # run the game
npx react-native build-godot         # build dist/index.bundle.js and its assets
npm run typecheck                    # tsc --noEmit
```

While `react_native/dev_server/enabled` is on, `run-godot` starts Metro unless it is already
running, and the game loads its bundle from Metro with Fast Refresh, LogBox and React Native
DevTools. Press **r** in Metro to reload and **j** to open DevTools. `run-godot --no-packager`
builds the bundle file and runs the game from it instead. Arguments after `--` go to Godot,
for example `npx react-native run-godot -- --headless`.

Exported games always load `react_native/bundle/path`, so build it before exporting. Export
templates that include this module are not published yet.

Add a `ReactNativeRootView` anywhere in a scene and set its `application_key` to a
registered component. All roots share one runtime. [Godot platform](godot-platform.md)
describes roots, threads and the developer server, and [interop](interop.md) describes how
roots talk to GDScript.
