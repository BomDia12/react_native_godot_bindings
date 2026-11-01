extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-storage"

var failed := false

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func done() -> void:
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	DirAccess.remove_absolute("user://react_native/async_storage/smoke.json")
	DirAccess.remove_absolute("user://react_native/async_storage/legacy/storage.json")
	if not await wait_until(func(): return await js("__godotStorage.createAsyncStorage != null"), "the bundle"):
		return
	await js("""__godotStorage.results = {};
	(async () => {
		const db = __godotStorage.createAsyncStorage('smoke');
		await db.setItem('volume', '0.5');
		await db.setItem('removed', 'x');
		await db.removeItem('removed');
		__godotStorage.results.volume = await db.getItem('volume');
		__godotStorage.results.missing = await db.getItem('missing');
		__godotStorage.results.keys = await db.getAllKeys();
		await __godotStorage.AsyncStorage.setItem('legacy', JSON.stringify({a: {b: 1}, c: 2}));
		await __godotStorage.native.legacy_multiMerge([['legacy', JSON.stringify({a: {d: 3}})]]);
		__godotStorage.results.merged = await __godotStorage.AsyncStorage.getItem('legacy');
		try {
			await __godotStorage.createAsyncStorage('../escape').setItem('a', 'b');
		} catch (error) {
			__godotStorage.results.escape = String(error.message);
		}
		__godotStorage.results.done = true;
	})().catch(error => { __godotStorage.results.error = String(error); __godotStorage.results.done = true; });
	""")
	if not await wait_until(func(): return await js("__godotStorage.results.done === true"), "the storage calls"):
		return
	var results: Dictionary = await js("__godotStorage.results")
	if results.has("error"):
		fail("storage failed: %s" % results.error)
		return
	if results.volume != "0.5" or results.missing != null or results.keys != ["volume"]:
		fail("the database returned %s" % results)
		return
	var merged: Dictionary = JSON.parse_string(results.merged)
	if merged.a.b != 1.0 or merged.a.d != 3.0 or merged.c != 2.0:
		fail("the merge did not go deep: %s" % results.merged)
		return
	if not results.has("escape") or FileAccess.file_exists("user://react_native/escape.json"):
		fail("a database name escaped the storage directory")
		return
	if not FileAccess.file_exists("user://react_native/async_storage/smoke.json") or not FileAccess.file_exists("user://react_native/async_storage/legacy/storage.json"):
		fail("the storage files are missing")
		return
	if FileAccess.file_exists("user://react_native/async_storage/smoke.json.tmp"):
		fail("a temporary file stayed behind")
		return

	# A new runtime reads what the old one wrote.
	var generation := ReactNativeHost.get_generation()
	ReactNativeHost.reload()
	if not await wait_until(func(): return ReactNativeHost.get_generation() != generation and await js("globalThis.__godotStorage != null"), "the reload"):
		return
	await js("__godotStorage.createAsyncStorage('smoke').getItem('volume').then(value => { __godotStorage.reloaded = value; })")
	if not await wait_until(func(): return await js("__godotStorage.reloaded === '0.5'"), "the value after a reload"):
		return
	done()
