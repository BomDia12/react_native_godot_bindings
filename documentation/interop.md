# JavaScript and Godot interop

JavaScript runs on React Native's JS thread and Godot runs on the main thread. Godot
TurboModules (`GodotScene`, `GodotAlert`, the application, network and image modules, and
[library](autolinking.md) modules such as `ExampleScene`) are implemented in C++ against
codegen'd specs, so React Native checks their argument and result types. Values that Godot scripts declare at runtime cross
through one schema-driven codec, which either converts a value completely or reports a
structured error; it never truncates, stringifies an unknown value, or silently
substitutes null.

## Threads and synchronous calls

TurboModule methods run on the JS thread. A method that needs Godot posts its work to
the main thread. Synchronous methods, such as `GodotScene.call`, block the JS thread until
that work finishes and then convert the result on the JS thread; asynchronous methods
return a Promise that settles later. The main thread never waits for JavaScript. Once the
host shuts down or a reload retires a runtime, that runtime's pending and new
synchronous calls fail with `E_CANCELLED`, and its queued Godot work is discarded. Nodes and scene-owned resources are only touched on the main thread; the
JS thread uses only `FileAccess`, `TextServer` and already-loaded fonts.

`ReactNativeHost.evaluate(code)` evaluates JavaScript from Godot without blocking. It
returns a signal that completes with the converted result:

```gdscript
var count = await ReactNativeHost.evaluate("globalThis.counter")
```

## Values

Supported values are null, booleans, finite numbers, safe integers, strings without an
embedded NUL, arrays, plain records, and `Uint8Array`. Typed wrappers represent `int64`,
`Color`, `Vector2`, `Vector3`, `Rect2`, `Transform2D` and registered objects.
`react-native-godot/values` builds them. `Uint8Array` uses its actual byte offset and
length and is copied into independent `PackedByteArray` storage. Full-range integers use
a canonical signed decimal wrapper:

```js
{$godot: 'int64', value: '9223372036854775807'}
```

`INTEGER` schemas accept values from -9,007,199,254,740,991 through
9,007,199,254,740,991. Use an `INT64` schema for larger signed 64-bit values. Without a
schema, exact safe JavaScript integers become Godot integers; other finite numbers,
including negative zero, stay floating point.

Functions, symbols, BigInt, sparse arrays, class instances, HostObjects, cycles,
non-finite numbers, unsafe integers, unknown wrappers and unsupported Variant kinds are
rejected. Each conversion allows at most 32 container levels, 4,096 fields or elements in
one container, 65,536 visited values, and 16 MiB of aggregate UTF-8 and binary payload.

## Errors

Godot errors reach JavaScript as thrown errors or Promise rejections with a `code` and
`message`. Stable codes are `E_VALIDATION`, `E_UNSUPPORTED`, `E_STALE_HANDLE`,
`E_OBJECT_GONE`, `E_CANCELLED`, `E_NATIVE` and `E_LIMIT`. A component name without a
Godot view prints `E_UNKNOWN_COMPONENT` to the Godot log once and mounts a plain View.
JavaScript errors and unhandled Promise rejections follow upstream React Native's error
handling; the host prints them and `ReactNativeHost.get_last_error()` returns the latest.

## Objects

JavaScript never supplies an ObjectID. Native code registers an existing Godot object
under a capability and returns an opaque handle that belongs to one root. Resolution
checks the root, the capability and the object's current lifetime on the main thread.
Destroying the object or stopping the root revokes its handles.

## Script capability resources

Attach an `RNSceneBinding` Resource with `root.attach_scene_binding(target, resource)`.
It returns an empty Dictionary on success or an error Dictionary; attachment is atomic.
Exported fields are `capability`, positive `schema_version`, `snapshot_method`,
`snapshot_schema`, `commands`, and `signals`. GDScript and `.tres` resources use the
same validated grammar. The target may precede tree entry; its binding is ready once
it is live inside the SceneTree. Leaving the tree/death/rebinding revokes old handles.

Value schemas are Dictionaries with a `type`: `void`, `dynamic`, `null`, `boolean`,
`number`, `integer`, `string`, `array`, `record`, `Uint8Array`, `int64`, `Color`,
`Vector2`, `Vector3`, `Rect2`, `Transform2D` or `Object`. `nullable` is
optional. Array schemas require `element`; records require `fields`, default to closed,
and can set `closed=false`. Record fields allow `optional` and a validated `default`.
Object schemas require a capability; Rect2 can allow negative size explicitly.
Unknown/misplaced options, cycles and structural/payload overflow fail before use.

Commands map a public name to `{method, mode, arguments, result}`. `mode` is `sync` or
`queued`. Ordered arguments use `{name, value, optional?, nullable?, default?}`;
omitted trailing arguments can use native script defaults. Required arguments must
precede optional/defaulted arguments. Nullable primitive and collection parameters
require untyped `Variant` script parameters; Object parameters may accept null. Signals map a native signal
to `{event, arguments, payload}`: argument names zip to a record payload. Methods,
arity, typed signatures, defaults and mapped signal fields are checked on attachment;
arguments/returns/emitted values are checked on every call. Only declared public methods
are callable; there is no unrestricted reflection from JS. Object-typed arguments
resolve their opaque handles against the calling root and declared capability
before invocation. Object results, snapshot fields and signal fields are registered
and returned as `{$godot: 'Object', handle}` wrappers, including declared array/record
fields and nullable values. Declared `int64` results preserve their decimal-string
wrappers through the scene response envelope. Godot retains ownership of the objects; destroyed objects
fail resolution and stopping the root invalidates their handles.

`react-native-godot/scene` exports `getBinding`, `read`, `call`, `callAsync`, `onChanged`
and `useGodotScene(rootTag)`; each takes the root tag of the calling surface. Responses
carry `ready`, an opaque `binding` handle, a monotonic `sequence`, `event`, `payload`,
`capability` and `schemaVersion`. `onChanged` delivers each change once. The hook waits
for readiness, re-reads the snapshot on later sequences and removes its subscription on
cleanup. Handles cannot cross roots. Scene calls run on Godot's main thread; a call that
reloads a root takes effect on the next frame.

A root whose component has a [contract](component-contracts.md) is its own binding:
`attach_scene_binding` fails on it with `E_VALIDATION`. Contract events are commands that
emit a signal instead of calling a method, and are always queued.

## Application modules

A module is a GDScript object, such as an autoload, that every root can use. It is declared
with the same `RNSceneBinding` grammar as a root's binding:

```gdscript
func _ready() -> void:
	var error := ReactNativeHost.register_module("Scoreboard", self, preload("res://scoreboard_binding.tres"))
	assert(error.is_empty(), str(error))
```

`register_module(name, target, binding)` returns an empty Dictionary or an error Dictionary,
and replaces an earlier module of the same name. `unregister_module(name)` removes it.
Modules belong to `ReactNativeHost`, not to a runtime, so they survive hard reloads.
Object handles a module returns belong to the module rather than to a root: any root can
use them, and they are revoked when the module is replaced or unregistered.

`react-native-godot/modules` exports `getGodotModule(name)`, which returns
`{ready, read(), call(command, ...args), callAsync(command, ...args), onChanged(callback)}`,
`onModuleChanged(name, callback)`, and `useGodotModule(name)`, which returns
`{module, value, sequence}` or `null` while the module isn't registered. Module changes carry
`module` instead of `rootTag`. Subscribing with `onModuleChanged` connects Godot to the
module's signals, as asking for the module does. A module registered again needs a fresh
`getGodotModule()`.

## Messages

Messages carry any value the codec carries, without a schema:

| Direction | Godot | JavaScript (`react-native-godot/messages`) |
|---|---|---|
| Godot to one root | `root.post_message(name, payload)` | `useGodotMessage(name, handler)`, or `onGodotMessage(name, handler, rootTag)` |
| Godot to the application | `ReactNativeHost.post_message(name, payload)` | `onGodotMessage(name, handler)` |
| one root to Godot | signal `root.message_received(name, payload)` | `usePostGodotMessage()(name, payload)`, or `postGodotMessage(name, payload, rootTag)` |
| the application to Godot | signal `ReactNativeHost.message_received(name, payload)` | `postGodotMessage(name, payload)` |

`useGodotMessage` and `usePostGodotMessage` address the root the component renders in.
Messages arrive in order and return nothing. JavaScript posts reach Godot on the main thread
on the next frame. A message for a root that isn't running is dropped with a warning, and
a payload holding an Object is dropped with an error, because Objects need a declared
capability. For the same reason, `postGodotMessage` throws `E_VALIDATION` for a payload
holding an Object wrapper.

## Binary and network ownership

Strings reject embedded NUL. Networking text uses byte transport, preserving NUL and
replacing lone UTF-16 surrogates or malformed UTF-8 with U+FFFD. Blobs live in the native
Blob service and follow upstream `BlobManager` ownership: native backing is released
after the last wrapper closes and its garbage collector notification drains on the main
thread. Native HTTP or WebSocket readers keep their own pins. As on iOS, an object URL
names a Blob range and holds no reference: `fetch` fails once the Blob is closed, and
`URL.revokeObjectURL` does nothing.

HTTPRequest nodes are lazy, bounded, always processing and shared by fetch/XHR/images.
A lease moves through IDLE → LEASED → DRAINING; cancellation releases no slot until the
old native completion/deferred drain barrier. Queue timeout includes waiting time;
zero means no caller deadline. Blob response descriptors preserve the case-insensitive
Content-Type response header; object URL responses carry no Content-Type. Metadata and data delivery share a response ownership
guard: listener failures release storage unless a Blob descriptor was successfully
handed off. Response headers use a prototype-free record, preserving names such as
`constructor` and `__proto__`; duplicate field names merge case-insensitively
under the first spelling. Image cancellation reports immediate release only for
queued work; active cancellation still completes its reservation. Credentials isolate
image cache/deduplication; no-store/no-cache stays uncacheable. Redirect hops apply cookies
and final URLs; query-only/fragment/relative references retain the current URL context
and normalize literal dot segments. Cross-origin sensitive headers are removed. Cookies are bounded and
in-memory and host-only. Set-Cookie headers with any Domain attribute are rejected;
parent-domain and public-suffix cookies are outside this subset. Path, Secure and expiry
rules still apply. Secure cookie prefixes are checked case-insensitively before
replacement: `__Secure-` requires Secure over HTTPS, and `__Host-` additionally requires
an explicit `Path=/` and no Domain attribute, following the
[cookie-prefix requirements](https://datatracker.ietf.org/doc/draft-ietf-httpbis-rfc6265bis/16/#section-5.6).
Ordinary Secure cookies also reject insecure replacement, deletion and child-path
overlays, using the asymmetric path/domain matching rules in the same specification.
Expired Secure entries do not block a new cookie. Oversized replacement values are
ignored before erasing an existing cookie; explicit expiry still deletes it. There is no browser persistence.

HTTPRequest uses Godot worker threads, bounded by the active request limit. Network
progress continues independently of render FPS. Completion callbacks are deferred onto
the main thread, and cancellation joins the worker before the lease drains.
`ReactNativeHost.get_service_stats()` reports pool and Blob usage.

WebSocketPeer owns protocol framing/TLS. Polling continues through CLOSING, with limits
on peers, messages, queues, retained bytes and packets delivered per frame. A message can
overshoot the per-frame byte target once so a legal packet cannot starve. Normal close,
failure, cancellation and generation reset release native peers once.
A socket reports open before its first message. Handshake, connect and packet failures
report `websocketFailed` once and release the peer. If a Blob-mode message cannot
allocate its binary backing, the socket fails the same way.

## Editor service limits

Limits below live under `react_native/`, expose editor ranges and are immutable per
generation. Invalid types/ranges, HTTP idle > active, or HTTP body > aggregate buffer
limits fail startup. A zero HTTP wait
queue permits immediate leases only; zero idle retires every completed node. Zero cookie
limits disable storage.

| Setting suffix | Default |
|---|---|
| network/http/max_active_requests / max_idle_requests / max_queued_requests | 8 / 2 / 128 |
| network/http/max_body_bytes / max_buffered_bytes / max_redirects | 8 MiB / 32 MiB / 8 |
| network/cookies/max_entries / max_bytes | 256 / 256 KiB |
| network/websocket/max_message_bytes / max_queued_packets / max_buffered_bytes | 1 MiB / 256 / 4 MiB |
| network/websocket/packets_per_frame / close_timeout_ms | 64 / 2000 |
| binary/max_blob_bytes | 32 MiB |
| appearance/color_scheme / follow_system | light / false |
| text/font_scale | 1.0 (changes apply at runtime) |
| network/tls/trusted_certificates | empty |

WebSocket message settings reserve codec envelope headroom below 16 MiB. Existing
image cache/decode/total ceilings and font aliases remain separate generation snapshots;
HTTP reservations account for image requests without taking ownership of image caches.
