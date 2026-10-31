# Autolinking

React Native Godot libraries are npm packages. Their JavaScript installs like any other
dependency, and `react-native autolink-godot` links their Godot side into the project:
native modules into the editor build, and addons into `res://addons/`.

```sh
npm install <library>
npx react-native autolink-godot
npx react-native build-godot-editor   # only when a library has a native module
```

## Library layout

```text
react-native-godot-example/
  package.json             codegenConfig: {name: "RNGodotExampleSpec", type: "all", jsSrcsDir: "specs"}
  react-native.config.js   dependency.platforms.godot: {module: "godot/rn_godot_example"}
  specs/                   codegen specs, as for other React Native platforms
  src/                     the library's JavaScript API
  godot/rn_godot_example/  a Godot module
  godot/addons/<name>/     a Godot addon
```

`react-native.config.js` names the Godot parts, relative to the package:

```js
module.exports = {
  dependency: {
    platforms: {
      godot: {module: 'godot/rn_godot_example', addon: 'godot/addons/godot_example'},
    },
  },
};
```

A library has a module, an addon, or both. The repository's
[`samples/libraries/`](../samples/libraries/) has one of each:
`react-native-godot-example` has a native component and a TurboModule, and
`react-native-godot-script-example` registers a GDScript [application
module](interop.md#application-modules).

### Native modules

A native module is a Godot module that depends on `react_native_bindings`. Its directory
name is the module name, which must be unique among the project's libraries; autolinking
rejects two modules with the same name.

```python
# config.py
def can_build(env, platform):
    env.module_add_dependencies("rn_godot_example", ["react_native_bindings"])
    return True


def configure(env):
    pass
```

```python
# SCsub
Import("env")
Import("env_react_native")

env_example = env_react_native.Clone()
env_example.add_source_files(env.modules_sources, "*.cpp")
```

`env_react_native` carries React Native's include paths, defines and flags, and the
codegen output of every linked library. Sources include the bindings' headers as
`react_native_bindings/...`, and their codegen as React Native's own builds lay it out:
`<react/renderer/components/RNGodotExampleSpec/Props.h>` and
`<RNGodotExampleSpec/RNGodotExampleSpecJSI.h>`. `register_types.cpp` adds the library's
components and TurboModules to [`RNExtensionRegistry`](extensions.md#registration) at
`MODULE_INITIALIZATION_LEVEL_SCENE`.

### Addons

An addon is a normal Godot addon with a `plugin.cfg`. A library whose objects JavaScript
reaches registers them from an autoload, which its plugin adds when it is enabled:

```gdscript
@tool
extends EditorPlugin

func _enable_plugin() -> void:
	add_autoload_singleton("ScriptExampleScoreboard", "res://addons/godot_script_example/scoreboard.gd")

func _disable_plugin() -> void:
	remove_autoload_singleton("ScriptExampleScoreboard")
```

Ship the `.uid` files Godot writes beside the addon's scripts, so every project refers to
them by the same UIDs.

## `react-native autolink-godot`

The command reads the libraries the React Native CLI finds in `node_modules`:

- For native modules, it runs codegen for each library that has a `codegenConfig`, into
  `node_modules/.cache/react-native-godot/autolinking/codegen/<library>/`, and writes
  `autolinking.json` there with the module directories, the codegen directories and a hash
  of the bindings commit, the library versions, the module sources and their codegen.
  Module directory names and codegen names must be unique, and codegen names can't be
  `FBReactNativeSpec`, `ReactNativeGodotSpec` or `rncore`, which React Native and the
  bindings use.
- It copies each addon to `res://addons/<directory>` and enables its plugin through the
  Godot editor, as Project Settings does, so `_enable_plugin` runs. When an update changes
  an enabled addon, its plugin is disabled before the copy and enabled after it, so the
  plugin sets itself up again. A removed library's plugin is disabled and its copy
  deleted. `godot-autolinking.lock.json` records the
  copies the command owns: it never replaces an `addons/` directory it didn't create, and
  two libraries can't link addons with the same directory name. Copies keep the `.uid` and
  `.import` files Godot adds to them.
- The plugin of a library that also has a native module may use the module's classes, so
  it is enabled once an editor with the module exists: `build-godot-editor` enables it.

Commit the addon copies, the lock file and `project.godot`. `--check` changes nothing
and exits with an error when the manifest, the codegen, the addon copies, the lock file or
the enabled plugins are out of date, or when native modules are linked and no editor is
built for them. Enabling plugins uses the editor in `--godot` or
`GODOT_EDITOR`, then the one built for the linked modules, then the prebuilt editor.

## Building the editor

Native modules need an editor compiled with them. `react-native build-godot-editor`
clones the bindings at the commit the installed `react-native-godot` was released from
into `~/.cache/react-native-godot/source/<commit>/`, and runs `bootstrap.sh`,
`build_hermes.sh` and `build_godot.sh` with `RN_AUTOLINKING` set to the project's
`autolinking.json`. It keeps the editor, `libhermesvm.so` and `libjsi.so` in
`~/.cache/react-native-godot/editors/<hash>/`. `--source <checkout>` builds from another
bindings checkout. The command builds Linux editors only, and warnings don't fail it.
Afterwards it links the project again with the new editor, which enables the plugins that
waited for it.

`run-godot`, `build-godot` and the other commands use the editor built for the project's
current hash, and ask for `build-godot-editor` when there is none. Projects without native
modules use the prebuilt editor. Run `autolink-godot`, then `build-godot-editor`, whenever a
native library is added, removed, or updated.

In this repository, `build_godot.sh` reads `RN_AUTOLINKING` the same way:

```sh
(cd samples/view-text && npx react-native autolink-godot)
RN_AUTOLINKING=samples/view-text/node_modules/.cache/react-native-godot/autolinking/autolinking.json scripts/build_godot.sh
```

## Exporting

Exported games run on export templates, which hold the engine and its modules, so a
library's native module reaches an exported game only through templates built with it.
Build `template_debug` and `template_release` from the same bindings checkout, with the
modules and option the editor build uses:

```sh
scons platform=linuxbsd target=template_release \
	custom_modules=<bindings>/modules,<each directory in autolinking.json's "modules"> \
	custom_modules_recursive=no react_native_autolinking=<path to autolinking.json>
```

Then set them as the export preset's custom templates. An export warns when the project
links native modules and the preset uses the default templates. Libraries without native
modules need nothing more: their addons are ordinary project files.
