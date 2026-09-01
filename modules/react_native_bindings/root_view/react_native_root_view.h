#pragma once

#include "../fabric/rn_registry.h"
#include "../fabric/rn_shadow_node.h"
#include "../input/rn_input_router.h"

#include "scene/gui/control.h"

#include <memory>
#include <deque>

class FabricUIManager;

class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	RNRegistry registry;
	int root_tag = 1;
	std::shared_ptr<FabricUIManager> ui_manager;
	Ref<RNShadowNode> committed_tree;
	RNInputRouter input_router;
	HashMap<int, Rect2> layout_cache;

	struct PendingCommit {
		Ref<RNShadowNode> tree;
		uint64_t generation = 0;
	};
	std::deque<PendingCommit> pending_commits;
	bool mount_flush_scheduled = false;
	int focused_tag = 0;
	bool booted = false;
	bool has_timers = false;

	void _boot();
	void _reload_from_source(const String &p_source);
	void _clear_children();
	void _cleanup_runtime_state(bool p_dispatch_cancellations);
	void _enqueue_events(const Vector<RNNativeEvent> &p_events, bool p_request_flush = true);
	void _on_react_native_file_changed(const String &p_path, const String &p_content, bool p_exists);
	void _on_focus_entered(int p_tag, ObjectID p_control_id);
	void _on_focus_exited(int p_tag, ObjectID p_control_id);
	void _set_focused_tag(int p_tag);

	void _layout_and_mount();
	Control *_build_node(const Ref<RNShadowNode> &p_node, Control *p_parent, bool p_branch_targetable = true);
	void _queue_layout_events(const Ref<RNShadowNode> &p_node, HashMap<int, Rect2> &r_next_cache, uint64_t p_generation);
	static bool _find_page_position(const Ref<RNShadowNode> &p_node, int p_tag, const Point2 &p_parent_position, Point2 &r_position);
	void _route_input(const Ref<InputEvent> &p_event);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void input(const Ref<InputEvent> &p_event) override;
	void gui_input(const Ref<InputEvent> &p_event) override;

public:
	ReactNativeRootView();
	~ReactNativeRootView() override;

	int get_root_tag() const { return root_tag; }

	void mount(const Ref<RNShadowNode> &p_tree);
	void enqueue_mount(const Ref<RNShadowNode> &p_tree, uint64_t p_generation);
	void _flush_mounts();
	void _flush_native_events();
	bool get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const;
};
