#include "react_native_root_view.h"

#include "../fabric/fabric_ui_manager.h"
#include "../fabric/rn_layout.h"
#include "../singletons/hermes_runtime_singleton.h"
#include "../singletons/react_native_file_singleton.h"

#include "core/object/callable_mp.h"
#include "core/string/print_string.h"
#include "scene/gui/label.h"

namespace {
constexpr const char *RUN_APPLICATION_FUNCTION = "__godotRunApplication";
constexpr const char *FLUSH_TIMERS_FUNCTION = "__godotFlushTimers";
} //namespace

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

	// The UI manager outlives this node inside the runtime; make sure nothing it holds
	// can reach a freed node.
	registry.clear();
}

void ReactNativeRootView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("mount", "tree"), &ReactNativeRootView::mount);
	ClassDB::bind_method(D_METHOD("get_root_tag"), &ReactNativeRootView::get_root_tag);
}

void ReactNativeRootView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_boot();
		} break;

		case NOTIFICATION_PROCESS: {
			// JS timers are a host facility, not a language one: the bundle queues them
			// and we drain the queue once per frame. Anything React schedules — effects,
			// the Promise microtask that follows a render — arrives through here.
			if (has_timers) {
				HermesRuntimeSingleton *hermes = HermesRuntimeSingleton::get_singleton();
				if (hermes) {
					hermes->call_function(FLUSH_TIMERS_FUNCTION);
				}
			}
		} break;

		case NOTIFICATION_RESIZED: {
			// Flexbox depends on the available size, so a resize is a relayout.
			if (committed_tree.is_valid()) {
				_layout_and_mount();
			}
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
	ui_manager = std::make_shared<FabricUIManager>(get_instance_id());
	hermes->install_host_object(FabricUIManager::GLOBAL_NAME, ui_manager);
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
	// functions behind. install_host_object() reinstalls the UI manager for us.
	hermes->reset();
	registry.clear();
	committed_tree.unref();
	_clear_children();

	hermes->set_global("__godotRootTag", root_tag);
	hermes->evaluate(p_source, "godot://bundle.js");

	const String error = hermes->get_last_error();
	if (!error.is_empty()) {
		ERR_PRINT("ReactNativeRootView: " + error);
		return;
	}

	has_timers = hermes->get_global(FLUSH_TIMERS_FUNCTION).get_type() != Variant::NIL;
	set_process(has_timers);

	// A React Native bundle registers this and renders nothing until we call it.
	// Hand-written Fabric scripts do their own commit and never define it.
	if (hermes->get_global(RUN_APPLICATION_FUNCTION).get_type() != Variant::NIL) {
		Array args;
		args.push_back(root_tag);
		hermes->call_function(RUN_APPLICATION_FUNCTION, args);

		const String run_error = hermes->get_last_error();
		if (!run_error.is_empty()) {
			ERR_PRINT("ReactNativeRootView: " + run_error);
		}
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

void ReactNativeRootView::mount(const Ref<RNShadowNode> &p_tree) {
	committed_tree = p_tree;
	_layout_and_mount();
}

void ReactNativeRootView::_layout_and_mount() {
	if (committed_tree.is_null()) {
		return;
	}

	RNLayout::calculate(committed_tree, get_size());

	// Rebuild rather than diff. Correctness first: incremental reconciliation is a
	// later slice and is invisible at this scale.
	registry.clear();
	_clear_children();
	_build_children(committed_tree);
}

void ReactNativeRootView::_build_children(const Ref<RNShadowNode> &p_node) {
	if (p_node.is_null()) {
		return;
	}

	if (p_node->view_name == "RCTText") {
		Label *label = memnew(Label);
		label->set_text(p_node->collect_text());
		label->set_position(p_node->layout.position);
		label->set_size(p_node->layout.size);
		add_child(label);
		registry.register_node(p_node->tag, label);
		return; // Its children are text, folded into the label above.
	}

	for (const Ref<RNShadowNode> &child : p_node->children) {
		_build_children(child);
	}
}
