#include "react_native_root_view.h"

#include "../fabric/rn_scratch_host.h"
#include "../singletons/hermes_runtime_singleton.h"
#include "../singletons/react_native_file_singleton.h"

#include "core/object/callable_mp.h"
#include "core/string/print_string.h"
#include "scene/gui/label.h"

ReactNativeRootView::ReactNativeRootView() {
	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && !file_singleton->is_connected("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed))) {
		file_singleton->connect("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed));
	}
}

ReactNativeRootView::~ReactNativeRootView() {
	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && file_singleton->is_connected("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed))) {
		file_singleton->disconnect("react_native_file_changed", callable_mp(this, &ReactNativeRootView::_on_react_native_file_changed));
	}

	// The host object outlives this node inside the runtime; make sure nothing it holds
	// can reach a freed node.
	registry.clear();
}

void ReactNativeRootView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("mount_scratch_tree", "tree"), &ReactNativeRootView::mount_scratch_tree);
	ClassDB::bind_method(D_METHOD("get_root_tag"), &ReactNativeRootView::get_root_tag);
}

void ReactNativeRootView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_boot();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			registry.clear();
		} break;
	}
}

void ReactNativeRootView::_boot() {
	if (booted) {
		return;
	}

	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL_MSG(hermes, "ReactNativeRootView: HermesRuntime singleton is missing.");

	root_tag = registry.allocate_tag();
	scratch_host = std::make_shared<RNScratchHost>(get_instance_id());
	hermes->install_host_object(RNScratchHost::GLOBAL_NAME, scratch_host);
	booted = true;

	ReactNativeFileSingleton *file_singleton = ReactNativeFileSingleton::get_singleton();
	if (file_singleton && file_singleton->has_file()) {
		_reload_from_source(file_singleton->get_file_content());
	}
}

void ReactNativeRootView::_reload_from_source(const String &p_source) {
	if (p_source.is_empty()) {
		return;
	}

	HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
	ERR_FAIL_NULL(hermes);

	// A full reset is simpler than an incremental reload and leaves no stale host
	// functions behind. install_host_object() reinstalls the scratch host for us.
	hermes->reset();
	if (scratch_host) {
		scratch_host->reset_state();
	}
	registry.clear();
	_clear_children();
	root_tag = registry.allocate_tag();

	hermes->evaluate(String::utf8(RNScratchHost::JS_SHIM), "godot://scratch_shim.js");
	hermes->evaluate(p_source, "godot://bundle.js");

	const String error = hermes->get_last_error();
	if (!error.is_empty()) {
		ERR_PRINT("ReactNativeRootView: " + error);
	}
}

void ReactNativeRootView::_clear_children() {
	for (int i = get_child_count() - 1; i >= 0; --i) {
		Node *child = get_child(i);
		remove_child(child);
		child->queue_free();
	}
}

void ReactNativeRootView::_on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists) {
	(void)p_path;

	if (!booted || !p_exists || p_content.is_empty()) {
		return;
	}

	_reload_from_source(p_content);
}

String ReactNativeRootView::_collect_text(const Dictionary &p_node) {
	String result;

	const String view_name = p_node.get("viewName", String());
	if (view_name == "RCTRawText") {
		result += String(p_node.get("text", String()));
	}

	const Array children = p_node.get("children", Array());
	for (int i = 0; i < children.size(); ++i) {
		result += _collect_text(children[i]);
	}

	return result;
}

void ReactNativeRootView::_build_node(const Dictionary &p_node, float &r_offset_y) {
	const String view_name = p_node.get("viewName", String());

	if (view_name == "RCTText") {
		Label *label = memnew(Label);
		label->set_text(_collect_text(p_node));
		label->set_position(Vector2(0, r_offset_y));
		add_child(label);

		// Yoga owns positioning from Phase 4 on; this stacking is a placeholder.
		r_offset_y += label->get_minimum_size().y;

		registry.register_node(p_node.get("tag", 0), label);
		return;
	}

	const Array children = p_node.get("children", Array());
	for (int i = 0; i < children.size(); ++i) {
		_build_node(children[i], r_offset_y);
	}
}

void ReactNativeRootView::mount_scratch_tree(const Dictionary &p_tree) {
	_clear_children();

	float offset_y = 0.0f;
	_build_node(p_tree, offset_y);
}
