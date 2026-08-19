#pragma once

#include "../fabric/rn_registry.h"
#include "../fabric/rn_shadow_node.h"

#include "scene/gui/control.h"

#include <memory>

class FabricUIManager;

// Root of a React Native surface. Owns the tag registry, the committed shadow tree
// and the Godot children the renderer produces, and drives the JS reload cycle.
class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	RNRegistry registry;
	int root_tag = 1;
	std::shared_ptr<FabricUIManager> ui_manager;
	Ref<RNShadowNode> committed_tree;
	bool booted = false;
	bool has_timers = false;

	void _boot();
	void _reload_from_source(const String &p_source);
	void _clear_children();
	void _on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists);

	void _layout_and_mount();
	// Builds a Godot Control for p_node, attaches it under p_parent, and recurses.
	// Returns the created Control (or nullptr for nodes that mount nothing, e.g. the
	// synthetic RCTRootView wrapper and stray raw text).
	Control *_build_node(const Ref<RNShadowNode> &p_node, Control *p_parent);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	ReactNativeRootView();
	~ReactNativeRootView() override;

	RNRegistry &get_registry() { return registry; }
	int get_root_tag() const { return root_tag; }

	// Called via call_deferred from completeRoot, after the runtime lock is released.
	void mount(const Ref<RNShadowNode> &p_tree);
};
