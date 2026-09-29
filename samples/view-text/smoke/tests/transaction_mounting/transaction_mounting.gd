extends ReactNativeRootView

const TIMEOUT_FRAMES := 600

var frames := 0
var stage := 0
var stage_frame := 0
var original_ids := {}
var did_resize := false
var rejected_props_sent := false

func fail(message: String) -> void:
	printerr("RN_SMOKE_FAILED: transaction-mounting: %s" % message)
	get_tree().quit(1)

func action(name: String) -> void:
	HermesRuntime.call_function("__godotTransactionAction", [name])
	stage += 1
	stage_frame = frames

func find_label(node: Node, text: String) -> Label:
	if node is Label and node.text == text:
		return node
	for child in node.get_children():
		var found := find_label(child, text)
		if found != null:
			return found
	return null

func item_panel(text: String) -> Panel:
	var label := find_label(self, text)
	return label.get_parent() as Panel if label != null else null

func item_order() -> Array[String]:
	var alpha := item_panel("alpha")
	if alpha == null:
		alpha = item_panel("alpha*")
	if alpha == null:
		return []
	var result: Array[String] = []
	for child in alpha.get_parent().get_children():
		if not child is Panel:
			continue
		for grandchild in child.get_children():
			if grandchild is Label:
				result.append(grandchild.text)
	return result

func record_id(text: String) -> void:
	var panel := item_panel(text)
	var label := find_label(self, text)
	original_ids[text.trim_suffix("*")] = [panel.get_instance_id(), label.get_instance_id()]

func identity_matches(text: String) -> bool:
	var key := text.trim_suffix("*")
	var panel := item_panel(text)
	var label := find_label(self, text)
	return panel != null and label != null and original_ids.get(key, []) == [panel.get_instance_id(), label.get_instance_id()]

func _process(_delta: float) -> void:
	frames += 1
	if frames > TIMEOUT_FRAMES:
		fail("timed out at stage %d" % stage)
		return
	if not has_node("AuthoredChild"):
		fail("renderer deleted the authored Godot child")
		return

	match stage:
		0:
			if item_order() != ["alpha", "beta", "gamma"]:
				return
			record_id("alpha")
			record_id("beta")
			record_id("gamma")
			action("update-leaf")
		1:
			if item_panel("alpha*") == null:
				return
			if not identity_matches("alpha*") or not identity_matches("beta") or not identity_matches("gamma"):
				fail("leaf update replaced a retained host")
				return
			action("reorder")
		2:
			if item_order() != ["gamma", "alpha*", "beta"]:
				return
			if not identity_matches("alpha*") or not identity_matches("beta") or not identity_matches("gamma"):
				fail("sibling reorder replaced a retained host")
				return
			action("remove")
		3:
			if item_panel("beta") != null or item_order() != ["gamma", "alpha*"]:
				return
			if not identity_matches("alpha*") or not identity_matches("gamma"):
				fail("sibling removal replaced a survivor")
				return
			action("insert")
		4:
			if item_order() != ["gamma", "alpha*", "delta"]:
				return
			var alpha := item_panel("alpha*")
			if not identity_matches("alpha*") or not identity_matches("gamma"):
				fail("sibling insertion replaced a survivor")
				return
			alpha.grab_focus()
			set_mount_failure_injection(-1, 0)
			action("style")
		5:
			var alpha := item_panel("alpha*")
			if frames - stage_frame < 3:
				return
			if item_order() != ["gamma", "alpha*", "delta"] or alpha == null or not is_equal_approx(alpha.size.x, 56.0) or not identity_matches("alpha*") or not identity_matches("gamma") or get_viewport().gui_get_focus_owner() != alpha:
				fail("rejected transaction changed the published focused host")
				return
			if not rejected_props_sent:
				HermesRuntime.call_function("__godotTransactionPropsAfterFailure")
				rejected_props_sent = true
				stage_frame = frames
				return
			if frames - stage_frame < 2:
				return
			if not is_equal_approx(alpha.modulate.a, 1.0):
				fail("direct props requiring the rejected revision reached the old host")
				return
			set_mount_failure_injection(-1, -1)
			action("recover")
		6:
			var alpha := item_panel("alpha*")
			if item_order() != ["alpha*", "gamma", "delta"] or alpha == null or not is_equal_approx(alpha.size.x, 72.0):
				return
			if not identity_matches("alpha*") or get_viewport().gui_get_focus_owner() != alpha:
				fail("later commit did not recover the retained focused host")
				return
			size.x += 40.0
			did_resize = true
			stage += 1
			stage_frame = frames
		7:
			if not did_resize or frames == stage_frame:
				return
			if not identity_matches("alpha*") or not identity_matches("gamma"):
				fail("root resize replaced a retained host")
				return
			print("RN_SMOKE_OK: transaction-mounting")
			get_tree().quit(0)
