# Godot-owned game UI

The game has three independent enemies and a 100-item inventory written in GDScript.
Each enemy owns an RN surface; the inventory owns a fourth. All share one Hermes
runtime and bundle. Godot signals update React directly, including with no server.

From the binding repository:

```sh
python3 -m pip install -r requirements-ci.txt
npm --prefix samples/game-ui ci
npm --prefix samples/game-ui run build:godot
python3 scripts/stage_game_demo.py
GODOT_SOURCE_DIR=${GODOT_SOURCE_DIR:-$PWD/godot}
"$GODOT_SOURCE_DIR/bin/godot.linuxbsd.editor.dev.x86_64" --editor --path ../godotProjects/Phase6BGame --import
"$GODOT_SOURCE_DIR/bin/godot.linuxbsd.editor.dev.x86_64" --path ../godotProjects/Phase6BGame
```

Press **1**, **2**, or **3** to damage that enemy; **P** picks up a potion. Click inventory
rows to use their items. The inventory shows FlatList/SectionList, local icons, native
scrolling/editors, controlled editing, theme/font changes, a Modal, Alert and Window.
Enter an optional HTTP endpoint and click **Refresh** to import inventory data into
Godot. Offline play requires no endpoint. Prefer uncontrolled TextInput `defaultValue`
for ordinary game entry; controlled `value` remains available with `onChangeText`.

`samples/game-ui` is the versioned source. `stage_game_demo.py` copies its built,
standalone Godot project to `godotProjects/Phase6BGame`, without Node dependencies.
Rebuild and stage after changing React source. The `.tres`/GDScript capability schemas
are project code; no enemy or inventory rules are compiled into the module.

The local network fixture starts and stops automatically in the HTTP/WebSocket smoke
manifests. It binds ephemeral localhost ports and uses a fixture CA for verified TLS:

```sh
GODOT_SOURCE_DIR="$GODOT_SOURCE_DIR" python3 scripts/run_smoke_tests.py
```

The original gallery remains available with `npm --prefix samples/game-ui run
build:gallery`; its scene is `res://smoke/tests/component_contracts/SmokeMain.tscn`.
The common smoke suite builds both bundles and runs every manifest once.

Limits are editable under **Project Settings → Advanced → React Native** and are
snapshotted on bundle generation. `react_native/text/font_scale` defaults to 1;
`react_native/appearance/color_scheme` defaults to light. Host appearance following is
opt-in. Native Godot themes/imported fonts remain the default styling inputs.

Linux editor/headless is the tested boundary. Headless Window tests exercise logical
content/lifetime; the display gate checks native windows separately. RefreshControl,
advanced animation/accessibility/export fidelity and actual IME candidate windows
remain pending.
