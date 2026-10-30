extends "res://smoke/communication_case.gd"

# Contract properties set from GDScript reach React as props, and contract
# events from React reach GDScript as typed signals.

var marks: Array = []
var heals: Array = []

func smoke_id() -> String:
	return "game-contract-props"

func props(component: String) -> Dictionary:
	var contracts = await js("__godotContracts")
	return contracts.get(component, {}) if contracts is Dictionary else {}

func run() -> void:
	var bar := ReactNativeRootView.new()
	bar.application_key = "EnemyHealthBar"
	bar.set("display_name", "Goblin")
	bar.set("health", 3)
	bar.size = Vector2(300, 60)
	add_child(bar)
	if not await wait_until(func(): return (await props("EnemyHealthBar")).get("displayName") == "Goblin", "the initial contract props"):
		return
	if (await props("EnemyHealthBar")).get("health") != 3:
		fail("the surface did not start with the contract values")
		return
	bar.set("health", 2)
	bar.set("max_health", 5)
	bar.set("tint", Color(1, 0, 0))
	if not await wait_until(func(): return (await props("EnemyHealthBar")).get("maxHealth") == 5, "contract changes"):
		return
	var current := await props("EnemyHealthBar")
	if current.get("health") != 2 or current.get("tint") != Color(1, 0, 0):
		fail("one frame of writes did not arrive together: %s" % current)
		return
	if not (bar.attach_scene_binding(self, RNSceneBinding.new()).get("code") == "E_VALIDATION"):
		fail("a contract root accepted another scene binding")
		return

	var roster := add_root("EnemyRoster")
	roster.connect("mark", func(enemy_id): marks.append(enemy_id))
	roster.connect("heal", func(enemy_id, amount): heals.append([enemy_id, amount]))
	roster.set("enemies", [{"id": "e1", "name": "Goblin", "health": 3, "max_health": 5}, {"id": "e2", "name": "Orc", "health": 7, "max_health": 9}])
	if not await wait_until(func(): return (await props("EnemyRoster")).get("enemies", []).size() == 2, "the roster"):
		return
	if (await props("EnemyRoster")).enemies[1].get("maxHealth") != 9:
		fail("record fields did not arrive in camelCase")
		return
	await js("__godotRosterEvents.onMark('e2')")
	await js("__godotRosterEvents.onHeal('e1', 4)")
	if not await wait_until(func(): return marks == ["e2"] and heals.size() == 1, "the event signals"):
		return
	if heals[0][0] != "e1" or typeof(heals[0][1]) != TYPE_INT or heals[0][1] != 4:
		fail("the heal signal arguments were %s" % [heals[0]])
		return

	var shadowing := GDScript.new()
	shadowing.source_code = "extends ReactNativeRootView\nsignal mark(amount: int)\n"
	shadowing.reload()
	roster.set_script(shadowing)
	if roster.get("enemies") != null:
		fail("a script signal that shadows a contract event kept the contract")
		return
	var shadowing_property := GDScript.new()
	shadowing_property.source_code = "extends ReactNativeRootView\nvar marked := 0\n"
	shadowing_property.reload()
	roster.set_script(shadowing_property)
	if roster.get("enemies") != null:
		fail("a script property that shadows a contract property kept the contract")
		return
	roster.set_script(null)
	if roster.get("enemies") == null:
		fail("removing the shadowing script did not restore the contract")
		return
	roster.set("marked", "e1")
	await wait_until(func(): return (await props("EnemyRoster")).get("marked") == "e1", "the restored contract")
