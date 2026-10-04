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

`INTEGER` schemas accept values from -9,007,199,254,740,991 through
9,007,199,254,740,991, including native argument and record-field defaults. Use an
`INT64` schema for larger signed 64-bit values. Defaults use the same nullability as
explicit arguments or record fields: either the value schema or the argument/field
may allow null.

A `DYNAMIC` schema infers Godot integers for exact safe JavaScript integers. Other
finite numbers, including negative zero, retain their floating-point representation.

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

Native strings exceeding the aggregate payload ceiling report `E_LIMIT`; embedded NUL
characters report `E_VALIDATION`.

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
run a microtask checkpoint after each callback. Late or duplicate completions are ignored.

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

## Component service boundary

Component events and GodotImageLoader reuse the existing schema registry and native
completion queue. Image source records and queryCache arrays are validated before
native casts. The shared HTTP transport is installed before bundle execution. Generic scene bindings
provide direct script snapshots, declared commands and signals; the working game uses
these without a network hop.

## Script capability resources

Attach an `RNSceneBinding` Resource with `root.attach_scene_binding(target, resource)`.
It returns an empty Dictionary on success or an error Dictionary; attachment is atomic.
Exported fields are `capability`, positive `schema_version`, `snapshot_method`,
`snapshot_schema`, `commands`, and `signals`. GDScript and `.tres` resources use the
same validated grammar. The target may precede tree entry; its binding is ready once
it is live inside the SceneTree. Leaving the tree/death/rebinding revokes old handles.

Value schemas are Dictionaries with a `type`: `void`, `dynamic`, `null`, `boolean`,
`number`, `integer`, `string`, `array`, `record`, `Uint8Array`, `int64`, `Color`,
`Vector2`, `Vector3`, `Rect2`, `Transform2D`, `Object`, or `Session`. `nullable` is
optional. Array schemas require `element`; records require `fields`, default to closed,
and can set `closed=false`. Record fields allow `optional` and a validated `default`.
Object schemas require a capability; Rect2 can allow negative size explicitly.
Unknown/misplaced options, cycles and structural/payload overflow fail before use.

Commands map a public name to `{method, mode, arguments, result}`. `mode` is `sync` or
`queued`. Ordered arguments use `{name, value, optional?, nullable?, default?}`;
omitted trailing arguments can use native script defaults. Signals map a native signal
to `{event, arguments, payload}`: argument names zip to a record payload. Methods,
arity, typed signatures, defaults and mapped signal fields are checked on attachment;
arguments/returns/emitted values are checked on every call. Only declared public methods
are callable; there is no unrestricted reflection from JS. Object-typed arguments
resolve their opaque handles against the current session and declared capability
before invocation. Object results, snapshot fields and signal fields are registered
and returned as `{$godot: 'Object', handle}` wrappers, including declared array/record
fields and nullable values. Declared `int64` results preserve their decimal-string
wrappers through the scene response envelope. Godot retains ownership of the objects; destroyed objects
fail resolution and session closure invalidates their handles.

`react-native-godot/scene` exports `getBinding`, `read`, `call`, `callAsync`, `onChanged`
and `useGodotScene(rootTag)`. Responses carry `ready`, opaque `binding`, monotonic
`sequence`, `event`, `payload`, capability and schemaVersion. Subscribe, look up and read;
ignore older sequences. The hook waits for readiness, re-reads snapshots and closes its
session/subscription on cleanup. When the event queue is full, each session retains
one resynchronization flag. A later `resync` notification carries current readiness and
handle with a null payload; consumers re-read the snapshot. Intermediate signal payloads
may coalesce under backpressure. Handles cannot cross sessions, even on the same root.
All scene operations run on Godot's main thread. A sync script can trigger root reload;
the coordinator defers runtime lifecycle work until the current Hermes call returns.

Application state events retain at most one latest notification per dimensions,
appearance, app state, keyboard and focus channel while the native queue is full. Native
layout invalidation proceeds immediately; notification retries do not repeat it.

## Scheduling, binary and transport ownership

Timers use a generation-owned monotonic clock. Due callback IDs are snapshotted;
nested registration waits for the next turn, cancellation applies immediately, missed
intervals skip catch-up bursts, thrown callbacks do not stop later work, and Hermes owns
microtasks. Tagged RN supplies `setImmediate`/`clearImmediate` through its bridgeless
`immediateShim`, backed by the native Hermes microtask queue; cancellation, callback
arguments and nested immediates are exercised in the real application. Recurring timers rotate behind waiting due callbacks to prevent starvation.
rAF runs only with a visible eligible root. Timer/native delivery drains are
bounded; idle deadlines cap configured work by remaining frame time (configured frame
rate, or a 60 Hz budget when uncapped). Explicit idle timeout zero is immediately
expired; an omitted timeout waits for spare frame time. Once all roots are removed,
the coordinator drains outstanding work and disconnects frame polling; new root entry
reconnects it. Game physics pause does not stop service time.
Root-origin scopes are retained for timers and listener registrations; microtasks begin
unattributed, so asynchronous app flows should retain `useGodotAlert()` explicitly.

General interop strings reject embedded NUL. Networking text uses byte transport and JS
UTF-8 encoding/decoding, preserving NUL and replacing lone UTF-16 surrogates or
malformed UTF-8 with U+FFFD. Binary facades never enlarge
the 16 MiB codec ceiling: native read/append chunks are at most 1 MiB. FileReader yields
between chunks, supports abort and pins native backing until completion/cancellation.
Blob collectors enqueue an atomic release marker without touching SceneTree/Hermes;
the main thread reclaims it after the final collector is gone. Collectors sharing a
blobId use an atomic reference count; a new collector created before the pending
release drains keeps the backing alive. Native pins remain independent.
Explicit close is idempotent; slices/clone collectors,
object URLs and native HTTP/socket readers retain backing through their own ownership.
Queued HTTP Blob and multipart uploads pin borrowed backing synchronously before
enqueue and unpin it on settlement, cancellation or submission failure. Closing the
source Blob before the next frame cannot invalidate an accepted upload.
Revoking a URL removes its pin. Reset closes all old-generation stores and collectors.

HTTPRequest nodes are lazy, bounded, always processing and shared by fetch/XHR/images.
A lease moves through IDLE → LEASED → DRAINING; cancellation releases no slot until the
old native completion/deferred drain barrier. Queue timeout includes waiting time;
zero means no caller deadline. Blob response descriptors preserve the case-insensitive
Content-Type response header, including synthetic responses for typed Blob object URLs. Metadata and data delivery share a response ownership
guard: listener failures release storage unless a Blob descriptor was successfully
handed off. Response headers use a prototype-free record, preserving names such as
`constructor` and `__proto__`. Image cancellation reports immediate release only for
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
Expired Secure entries do not block a new cookie. There is no browser persistence.

HTTPRequest uses Godot worker threads, bounded by the active request limit. Network
progress continues independently of render FPS. Completion callbacks are deferred onto
the main thread, and cancellation joins the worker before the lease drains. Requests
that complete during submission are never retained in the module ownership map;
`GodotHTTP.stats().ownedRequests` reports its current pending ownership count.

WebSocketPeer owns protocol framing/TLS. Polling continues through CLOSING, with limits
on peers, messages, queues, retained bytes and packets delivered per frame. A message can
overshoot the per-frame byte target once so a legal packet cannot starve. Normal close,
failure, cancellation and generation reset release native peers once.
Open notifications retry when the native event queue is full, before messages are delivered.
Handshake/connect/packet failures and close notifications retain the peer until terminal
delivery is accepted, then release it once. If a Blob-mode message cannot allocate or
finish its binary backing, the facade releases partial storage, closes the native peer
and emits `websocketFailed`; tagged RN turns that into one error and abnormal close.
Queued messages and the later native terminal event are suppressed for that socket.

## Editor service limits

Limits below live under `react_native/`, expose editor ranges and are immutable per
generation. Invalid types/ranges, HTTP idle > active, or HTTP body > aggregate buffer
limits fail startup. A zero HTTP wait
queue permits immediate leases only; zero idle retires every completed node. Zero cookie
limits disable storage and zero scheduler idle budget runs only expired idle timeouts.

| Setting suffix | Default |
|---|---|
| network/http/max_active_requests / max_idle_requests / max_queued_requests | 8 / 2 / 128 |
| network/http/max_body_bytes / max_buffered_bytes / max_redirects | 8 MiB / 32 MiB / 8 |
| network/cookies/max_entries / max_bytes | 256 / 256 KiB |
| network/websocket/max_message_bytes / max_queued_packets / max_buffered_bytes | 1 MiB / 256 / 4 MiB |
| network/websocket/packets_per_frame / close_timeout_ms | 64 / 2000 |
| binary/max_blob_bytes | 32 MiB |
| scheduler/max_tasks_per_frame / idle_budget_ms | 256 / 2 |
| appearance/color_scheme / follow_system | light / false |
| text/font_scale | 1.0 |

WebSocket message settings reserve codec envelope headroom below 16 MiB. Existing
image cache/decode/total ceilings and font aliases remain separate generation snapshots;
HTTP reservations account for image requests without taking ownership of image caches.
