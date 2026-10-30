# Component contracts

A component contract declares what a React component receives from Godot and what it sends
back. It is an `RNComponentContract` resource saved as
`<react_native/project/root>/contracts/<Component>.tres`, and it is the single source of
truth for both sides:

- A `ReactNativeRootView` whose `application_key` names the component shows the contract's
  properties in the inspector, under **Component**, and its events as signals.
- The build generates TypeScript from the contracts, so `tsc` reports a component whose props
  drift from its contract.

## The resource

| Property | Meaning |
|---|---|
| `component` | The registered component name, in PascalCase. |
| `properties` | Ordered `{name, value, default}` entries. `value` is a value schema, as in [interop](interop.md#script-capability-resources). |
| `events` | Ordered `{name, arguments: [{name, value}]}` entries. |
| `types` | Named record types, `{RosterEnemy: {type: "record", fields: {...}}}`, referenced from any schema as `{"type": "named", "name": "RosterEnemy"}`. |

```gdscript
# res://react_native/contracts/EnemyHealthBar.tres, as text
component = "EnemyHealthBar"
properties = [
	{"name": "display_name", "value": {"type": "string"}, "default": ""},
	{"name": "health", "value": {"type": "integer"}, "default": 0},
	{"name": "tint", "value": {"type": "Color"}, "default": Color(1, 1, 1)},
]
events = [{"name": "mark", "arguments": [{"name": "enemy_id", "value": {"type": "string"}}]}]
```

Property, event, argument and record field names are snake_case. Property and event names
must be unique and can't reuse a property, method or signal of `ReactNativeRootView`, nor
a property or signal of the root's script. Every property needs a default that fits its
schema. Contract values start the surface as plain props, so they can't use `int64`,
`Uint8Array` or `Object` schemas; pass objects through a scene binding or an
[application module](interop.md#application-modules). Field names convert between
snake_case and camelCase by the schema, so values can't be `dynamic` either; send
free-form values as [messages](interop.md#messages). Named types can't contain themselves,
directly or through fields and arrays. A contract file is named after its component:
`contracts/EnemyRoster.tres` must declare `component = "EnemyRoster"`. An invalid contract
reports why when its root loads it, and the root then behaves as if the component had no
contract.

## From Godot

```gdscript
$HealthBar.health = enemy.health
$HealthBar.tint = Color.RED
$Roster.mark.connect(func(enemy_id: String): session.mark(enemy_id))
```

Values are checked against their schemas; a value that doesn't fit is rejected with an error
that names the property. Writes made during one frame reach React together at the end of
that frame, through the root's `contract_changed(values)` signal. Values are saved with the
scene in `contract_values`, so they survive a contract that is missing for a while. The
surface starts with the current values as its props, so the first frame shows them.

Event signals are user signals added when the root loads its contract, so connections made
in the editor save with the scene. Editing a contract updates every open root that uses it;
an event whose arguments change is added again, which drops its connections.
A root with a contract is its own scene binding, so `attach_scene_binding` fails with
`E_VALIDATION` on it.

## From React

The build writes three kinds of files under the project root:

| File | Owner | Content |
|---|---|---|
| `godot/contracts.ts` | generated | Named types, one `<Component>Props` type per contract, and the `GodotContracts` map. |
| `godot/registry.ts` | generated | Registers every contract component, wrapped by `withGodotContract`. The template's `index.js` imports it. |
| `components/<Component>.tsx` | developer | Written once, when missing, as a component that returns a `View`. |

```ts
// godot/contracts.ts
export type EnemyHealthBarProps = {
  displayName: string;
  health: number;
  tint: GodotColor;
  onMark: (enemyId: string) => void;
};
```

```tsx
// components/EnemyHealthBar.tsx
export default function EnemyHealthBar({displayName, health, onMark}: EnemyHealthBarProps) {
  return <Text onPress={() => onMark(displayName)}>{`${displayName} ${health}`}</Text>;
}
```

| Schema | TypeScript |
|---|---|
| `boolean`, `string` | `boolean`, `string` |
| `integer`, `number` | `number` |
| `Color`, `Vector2`, `Vector3`, `Rect2`, `Transform2D` | `GodotColor`, `GodotVector2`, `GodotVector3`, `GodotRect2`, `GodotTransform2D` |
| `array` | `readonly T[]` |
| `record`, named record | an object type with camelCase fields |
| `nullable` | `T \| null` |

Props and record fields arrive in camelCase, and event arguments are converted back to
snake_case. An underscore before a letter becomes its capital, and one before a digit
stays, so `max_health_2` becomes `maxHealth_2` and converts back unchanged. An event becomes an `on<Event>` prop that queues the signal's emission on the
main thread. `withGodotContract` from `react-native-godot/contracts` reads the values once
when the binding connects and then follows each change, without reading again.

## Generation

`react-native build-godot` and `react-native run-godot` regenerate the files whenever the
project has a `contracts` folder. Only the Godot editor can read the resources, so they run
`godot --headless --editor --path <project> --react-native-generate-contracts`, which exits
with an error when a contract is invalid. Pass `--godot` or set `GODOT_EDITOR` where the
prebuilt editor isn't available. Generated files are only written when their content
changes, so Metro and Fast Refresh see no spurious edits. While generating, roots don't start
React Native.
