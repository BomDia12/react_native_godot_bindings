# Editor

The module's editor plugin turns the Godot editor into the place where a React Native
project is configured, edited, run and debugged.

## Roots in the inspector

A selected `ReactNativeRootView` shows, above its properties, the bundle it runs and whether
the project uses the developer server, with a button that opens Project Settings.

`application_key` is a component picker. It lists the project's
[contract](component-contracts.md) components first, then the keys the editor's own runtime
registers (a "no contract" tooltip marks those), then:

- **Create new…**, which asks for a PascalCase name, typed properties and events
  (`name: type` pairs), saves `contracts/<Name>.tres`, generates the TypeScript and a first
  `components/<Name>.tsx`, and selects the new component;
- **Edit contract…**, which opens the selected component's contract in the inspector, where
  defaults and named record types are edited too.

Saving a contract regenerates the TypeScript. A root warns when its contract is invalid,
and when the generated TypeScript differs from what the contracts generate.

`fit_content`, `initial_properties` and `editor_preview` are described in
[Roots in a scene](godot-platform.md#roots-in-a-scene).

## The React Native panel

The **React Native** bottom panel manages Metro while `react_native/dev_server/enabled` is on.
It reuses a Metro that already answers on the configured host and port, and otherwise
starts the project's own with `react-native-godot-start`, which it stops with the editor. When
a reused Metro stops accepting connections, the panel starts the project's own in its place. A
headless editor, such as one running an import or an export, never starts Metro. The panel
shows Metro's state and log, and has:

- **Fast Refresh**, bound to `react_native/dev_server/fast_refresh`. Turning it off works like
  React Native's dev menu: Fast Refresh stops in the editor at once, without a reload, and
  saved edits apply at the next reload. A running game reads the setting when it starts.
- **Restart Metro**.
- **Reload**, which reloads the editor's preview and every running game through Metro.
- **Open DevTools**. The editor's runtime appears in React Native DevTools as
  `<project>-editor`, apart from the game.

The **Problems** tab lists `tsc --watch` diagnostics when the project has a `tsconfig.json`,
and the editor preview's last JavaScript error. Activating a TypeScript problem opens its
file at the line. `node` comes from the editor setting `react_native/tooling/node_path`.

## JavaScript and TypeScript files

The plugin adds `js`, `jsx`, `ts`, `tsx`, `mjs` and `cjs` to the editor setting
`docks/filesystem/textfile_extensions`, so the FileSystem dock lists React files and the
script editor opens them. They are highlighted with the editor theme's colors: keywords,
control flow, built-in types, numbers, strings, template literals and comments. JSX has no
highlighting of its own. Godot's editor has no JavaScript completion; use the
**Problems** tab or an external editor for type checking.

## Debugging

A game the editor runs sends its JavaScript errors and console output to a **React Native**
tab in the debugger. Error stacks are symbolicated: through Metro's `/symbolicate` for a
bundle Metro served, and from the `.map` file beside the bundle otherwise. Frames whose
sources are inside the project open in the script editor; others, such as
`node_modules`, are listed without a link. A `.map` file is read again when its content changes. The
tab clears when a new session starts.

## Exporting

Exports build a release bundle first, minified, with `react-native-godot/scripts/build.cjs`
and the project's `node`, after regenerating contracts. New assets are imported before the
export collects its files, and the bundle is added to the export because a `.js` file is not
a resource. Source maps, `.dependencies.json`, `assets-manifest.json` and the JavaScript
project's `package.json`, `package-lock.json` and `tsconfig.json` stay out. Debug exports
get the same release bundle; debug JavaScript in a game the editor runs. Export templates
ignore the developer server.

A failed build is reported as an export error, and the export then has no bundle. Godot
doesn't let an export plugin stop an export: the export dialog shows it as failed, but
`--export-release`, `--export-debug` and `--export-pack` print "completed with warnings"
and exit successfully, so scripts that export should treat that message as a failure.
