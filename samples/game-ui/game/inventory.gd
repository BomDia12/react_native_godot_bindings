extends Node

const Schema = preload("res://game/schemas.gd")
signal changed(state: Dictionary)

var items: Array = []
var revision := 0
var title := "Inventory"

func _init() -> void:
	for index in range(100):
		items.append({"id": "potion" if index == 0 else "item-%d" % index,
			"title": "Health potion" if index == 0 else "Item %d" % index,
			"quantity": 3 if index == 0 else 1, "equipped": false})

func snapshot() -> Dictionary:
	return {"items": items.duplicate(true), "revision": revision, "title": title}

func publish() -> Dictionary:
	revision += 1
	var state := snapshot()
	changed.emit(state)
	return state

func set_quantity(item_id: String, quantity: int) -> Dictionary:
	for item in items:
		if item.id == item_id:
			item.quantity = clampi(quantity, 0, 9999)
			return publish()
	return snapshot()

func use_item(item_id: String) -> Dictionary:
	for item in items:
		if item.id == item_id and item.quantity > 0:
			item.quantity -= 1
			return publish()
	return snapshot()

func equip(item_id: String) -> Dictionary:
	for item in items:
		if item.id == item_id:
			item.equipped = not item.equipped
			return publish()
	return snapshot()

func rename(value: String) -> Dictionary:
	title = value.left(80)
	return publish()

func network_update(update: Dictionary) -> Dictionary:
	if update.get("item") is String and update.get("quantity") is int:
		return set_quantity(update.item, update.quantity)
	return snapshot()

func import_items(definitions: Array) -> Dictionary:
	for definition in definitions:
		if definition is Dictionary and definition.get("id") is String and definition.get("quantity") is int:
			set_quantity(definition.id, definition.quantity)
	return snapshot()

func binding() -> RNSceneBinding:
	var item := Schema.record({"id": Schema.value("string"), "title": Schema.value("string"),
		"quantity": Schema.value("integer"), "equipped": Schema.value("boolean")})
	var state := Schema.record({"items": Schema.array(item), "revision": Schema.value("integer"), "title": Schema.value("string")})
	return Schema.binding("Inventory", state, {
		"use": Schema.command("use_item", [Schema.argument("id", Schema.value("string"))], state),
		"equip": Schema.command("equip", [Schema.argument("id", Schema.value("string"))], state),
		"rename": Schema.command("rename", [Schema.argument("title", Schema.value("string"))], state),
		"setQuantity": Schema.command("set_quantity", [Schema.argument("id", Schema.value("string")), Schema.argument("quantity", Schema.value("integer"))], state),
		"networkUpdate": Schema.command("network_update", [Schema.argument("update", Schema.record({"item": Schema.value("string"), "quantity": Schema.value("integer")}))], state, "queued"),
		"importItems": Schema.command("import_items", [Schema.argument("definitions", Schema.array(Schema.record({"id": Schema.value("string"), "title": Schema.value("string"), "quantity": Schema.value("integer")})))], state, "queued")})
