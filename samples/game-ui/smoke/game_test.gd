extends "res://game/game.gd"

const WAIT_MSEC := 30000

var failures: Array[String] = []

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

# Evaluates a JavaScript expression that may produce a promise and returns its
# settled value, or {"error": code} when it rejects.
func js_settled(expression: String) -> Variant:
	await js("globalThis.__settled=undefined;Promise.resolve().then(()=>%s).then(value=>{__settled={value};},error=>{__settled={error:error?.code??String(error)};});undefined;" % expression)
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		var settled = await js("__settled")
		if settled is Dictionary:
			return settled.get("value") if settled.has("value") else {"error": settled.error}
		await get_tree().process_frame
	return {"error": "timeout"}

func fixture() -> Dictionary:
	var value = await js("__game")
	return value if value is Dictionary else {}

func check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)

func root_action(root: ReactNativeRootView, name: String, arguments: Array = []) -> void:
	await js("__gameAction(%d, %s, %s)" % [root.get_root_tag(), JSON.stringify(name), JSON.stringify(arguments)])

func action(name: String, arguments: Array = []) -> void:
	await root_action(inventory_root, name, arguments)

func target(id: String, node: Node = null) -> Control:
	if node == null:
		node = self
	if node is Control and node.get_meta("react_native_test_id", "") == id:
		return node
	for child in node.get_children(true):
		var result := target(id, child)
		if result != null:
			return result
	return null

func frames(count: int) -> void:
	for i in count:
		await get_tree().process_frame

# Polls a condition, which may await, until it holds. Records a failure and
# returns false after WAIT_MSEC.
func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	failures.append("Timed out waiting for %s; events %s" % [label, (await fixture()).get("events", [])])
	return false

func game_ready() -> bool:
	var state := await fixture()
	return state.get("enemies", {}).size() == 3 and state.get("inventory") is Dictionary and target("inventory-editor") != null

# I18nManager.forceRTL persists in user data, so a run that ends early must not
# start the next one right to left.
const DIRECTION_PREFERENCES := "user://react_native_direction.cfg"

func _ready() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path(DIRECTION_PREFERENCES))
	super._ready()
	_start.call_deferred()

func _start() -> void:
	if await wait_until(game_ready, "the game UI"):
		await run()
	finish(smoke_id())

func smoke_id() -> String:
	return ""

func run() -> void:
	pass

func finish(id: String) -> void:
	for failure in failures:
		push_error(failure)
	if failures.is_empty():
		print("RN_SMOKE_OK: ", id)
	get_tree().quit(0 if failures.is_empty() else 1)
