# JavaScript and Godot interop

Native-module arguments, results, events, descriptor-owned typed props, and the public
Hermes value bridge use one schema-driven codec. Conversion either succeeds completely
or returns a structured error; it never truncates, stringifies an unknown value, or
silently substitutes null.

## Values

Supported values are null, booleans, finite numbers, safe integers, strings without an
embedded NUL, arrays, plain records, and `Uint8Array`. Typed wrappers represent `int64`,
`Color`, `Vector2`, `Vector3`, `Rect2`, `Transform2D`, registered objects, and sessions.
`Uint8Array` uses its actual byte offset and length and is copied into independent
`PackedByteArray` storage. Full-range integers use a canonical signed decimal wrapper:

```js
{$godot: 'int64', value: '9223372036854775807'}
```

Functions are not data values. Event subscriptions and internal microtasks validate and
retain JSI functions through dedicated callback paths. Symbols, BigInt, sparse arrays,
class instances, arbitrary HostObjects, cycles, non-finite numbers, unsafe integers,
unknown wrappers, and unsupported Godot Variant kinds are rejected.

Each conversion allows at most 32 container levels, 4,096 fields/elements in one
container, 65,536 visited values, and 16 MiB of aggregate UTF-8/binary payload. These are
safety ceilings. Large binary transfers should be chunked because validation and the
initial copy run while Hermes is owned. Test-only statistics expose visited values,
allocated containers, and copied bytes; the byte path performs one payload-sized copy.

## Errors

Errors carry `code`, `message`, `operation`, and `path`, plus relevant module,
component, root, tag, generation, or revision context. Stable codes are
`E_VALIDATION`, `E_UNSUPPORTED`, `E_UNKNOWN_COMPONENT`, `E_UNKNOWN_MODULE`,
`E_DUPLICATE_REGISTRATION`, `E_STALE_HANDLE`, `E_OBJECT_GONE`, `E_CANCELLED`,
`E_SESSION_CLOSED`, `E_RUNTIME_RESET`, `E_NATIVE`, `E_UNHANDLED_REJECTION`, and
`E_LIMIT`. Sync methods throw this shape; async methods reject with it.

Unhandled Promise rejections are reported through the native diagnostic path in both
production and development bundles. A rejection handled later produces the matching
handled record. Runtime reset is the exception: the old realm is destroyed, so its
Promises cannot observe a final rejection.

## Objects, sessions, and cancellation

JavaScript never supplies an ObjectID. Native code registers an existing Godot object
under a capability and receives an opaque generation-bound token. Resolution checks the
generation, session, root/surface epoch, capability, and current ObjectDB lifetime on the
main thread. Destroying an object, closing a session, removing a surface, or resetting
Hermes revokes its handles.

One module instance is created lazily per Hermes generation and shared by roots. Work
that touches a scene opens an explicit root session. A frame processes copied native
jobs, queues copied completions/events, then reacquires Hermes to deliver callbacks and
run the outer microtask checkpoint. Late or duplicate completions are ignored.

`callAsync(module, method, args, {signal})` in `js/godot/modules.js` connects an
`AbortSignal` to the native request token. An already-aborted signal rejects with
`E_CANCELLED`; later abort, session closure, surface removal, and runtime reset settle or
discard work once according to their lifetime. Subscription callbacks remain strong JSI
references until `{remove()}`, session/surface closure, or reset.

Event delivery snapshots the matching subscription tokens for each event. Callbacks may
remove themselves or other subscriptions, close their session, or add subscriptions.
Removed subscriptions are skipped immediately, and new subscriptions begin with the
next event. The running callback remains retained until it returns.

## Local resource paths

Built-in local file/import operations accept normalized `res://` and `user://` paths
only. Backslashes, absolute OS paths, empty paths, other schemes, `..` segments, and
embedded NUL are rejected. `.` and repeated separator segments are normalized once;
case and literal percent characters are preserved. Godot's FileAccess/ResourceLoader
performs the final virtual-path lookup. A custom native import resolver is a trusted
extension point and is outside this built-in rule.
