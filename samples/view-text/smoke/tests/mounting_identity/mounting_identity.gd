extends ReactNativeRootView

const TIMEOUT_FRAMES := 300

var original_ids := {}

func fail(message: String) -> void:
	printerr("RN_SMOKE_FAILED: mounting-identity: %s" % message)
	get_tree().quit(1)

func action(name: String) -> void:
	await ReactNativeHost.evaluate("__godotTransactionAction(%s)" % JSON.stringify(name))

func wait_until(condition: Callable, label: String) -> bool:
	for i in TIMEOUT_FRAMES:
		if not has_node("AuthoredChild"):
			fail("mounting deleted the authored Godot child")
			return false
		if condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func find_label(node: Node, text: String) -> RNTextControl:
	if node is RNTextControl and node.get_text() == text:
		return node
	for child in node.get_children():
		var found := find_label(child, text)
		if found != null:
			return found
	return null

# Upstream flattening hoists each item's Text next to its bordered View, so a
# panel is matched to its label by geometry.
func item_panel(text: String) -> Panel:
	var label := find_label(get_child(0, true), text)
	if label == null:
		return null
	if label.get_parent() is Panel:
		return label.get_parent()
	for child in label.get_parent().get_children():
		if child is Panel and child.get_rect().encloses(label.get_rect()):
			return child
	return null

func collect_labels(node: Node, labels: Array[RNTextControl]) -> void:
	for child in node.get_children():
		if child is RNTextControl:
			labels.append(child)
		collect_labels(child, labels)

func item_order() -> Array[String]:
	var labels: Array[RNTextControl] = []
	collect_labels(get_child(0, true), labels)
	labels.sort_custom(func(a, b): return a.global_position.x < b.global_position.x)
	var result: Array[String] = []
	for label in labels:
		result.append(label.get_text())
	return result

func record_id(text: String) -> void:
	original_ids[text.trim_suffix("*")] = [item_panel(text).get_instance_id(), find_label(get_child(0, true), text).get_instance_id()]

func identity_matches(texts: Array) -> bool:
	for text in texts:
		var panel := item_panel(text)
		var label := find_label(get_child(0, true), text)
		if panel == null or label == null or original_ids.get(text.trim_suffix("*"), []) != [panel.get_instance_id(), label.get_instance_id()]:
			return false
	return true

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	if not await wait_until(func(): return item_order() == ["alpha", "beta", "gamma"], "the initial items"):
		return
	for text in ["alpha", "beta", "gamma"]:
		record_id(text)

	await action("update-leaf")
	if not await wait_until(func(): return item_panel("alpha*") != null, "the leaf update"):
		return
	if not identity_matches(["alpha*", "beta", "gamma"]):
		fail("leaf update replaced a retained host")
		return

	await action("reorder")
	if not await wait_until(func(): return item_order() == ["gamma", "alpha*", "beta"], "the reorder"):
		return
	if not identity_matches(["alpha*", "beta", "gamma"]):
		fail("sibling reorder replaced a retained host")
		return

	await action("remove")
	if not await wait_until(func(): return item_order() == ["gamma", "alpha*"], "the removal"):
		return
	if not identity_matches(["alpha*", "gamma"]):
		fail("sibling removal replaced a survivor")
		return

	await action("insert")
	if not await wait_until(func(): return item_order() == ["gamma", "alpha*", "delta"], "the insertion"):
		return
	if not identity_matches(["alpha*", "gamma"]):
		fail("sibling insertion replaced a survivor")
		return

	item_panel("alpha*").grab_focus()
	await action("style")
	if not await wait_until(func(): return item_order() == ["alpha*", "gamma", "delta"] and is_equal_approx(item_panel("alpha*").size.x, 72.0), "the style update"):
		return
	if not identity_matches(["alpha*", "gamma"]) or get_viewport().gui_get_focus_owner() != item_panel("alpha*"):
		fail("style update replaced the focused host: identity=%s focus=%s panel=%s" % [identity_matches(["alpha*", "gamma"]), get_viewport().gui_get_focus_owner(), item_panel("alpha*")])
		return

	await ReactNativeHost.evaluate("__godotTransactionSetNativeProps()")
	if not await wait_until(func(): return is_equal_approx(item_panel("alpha*").modulate.a, 0.4), "setNativeProps"):
		return

	size.x += 40.0
	await get_tree().process_frame
	await get_tree().process_frame
	if not identity_matches(["alpha*", "gamma"]):
		fail("root resize replaced a retained host")
		return
	print("RN_SMOKE_OK: mounting-identity")
	get_tree().quit(0)
