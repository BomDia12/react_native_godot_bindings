#pragma once

#include "../fabric/rn_registry.h"

#include "scene/gui/control.h"

#include <memory>

class RNScratchHost;

// Root of a React Native surface. Owns the tag registry and the Godot children the
// renderer produces, and drives the JS reload cycle.
class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	RNRegistry registry;
	int root_tag = 0;
	std::shared_ptr<RNScratchHost> scratch_host;
	bool booted = false;

	void _boot();
	void _reload_from_source(const String &p_source);
	void _clear_children();
	void _on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists);

	void _build_node(const Dictionary &p_node, float &r_offset_y);
	static String _collect_text(const Dictionary &p_node);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	ReactNativeRootView();
	~ReactNativeRootView() override;

	RNRegistry &get_registry() { return registry; }
	int get_root_tag() const { return root_tag; }

	// Called via call_deferred from the JS thread, after the runtime lock is released.
	void mount_scratch_tree(const Dictionary &p_tree);
};
