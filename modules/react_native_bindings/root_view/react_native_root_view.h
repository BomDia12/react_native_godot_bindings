#pragma once

#include "../fabric/rn_registry.h"
#include "../fabric/rn_shadow_node.h"
#include "../input/rn_input_router.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "scene/gui/control.h"

class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	RNRegistry registry;
	RNInputRouter input_router;
	Ref<RNShadowNode> committed_tree;
	HashMap<int, Rect2> layout_cache;
	HashMap<int, Dictionary> direct_prop_overrides;
	String application_key = "GodotApp";
	int root_tag = 0;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	uint64_t mounted_revision = 0;
	int focused_tag = 0;
	bool registered = false;
	bool replacing_tree = false;

	void _clear_children();
	void _clear_scene_state();
	void _enqueue_events(Vector<RNNativeEvent> p_events);
	void _stamp_events(Vector<RNNativeEvent> &r_events) const;
	void _on_focus_entered(int p_tag, ObjectID p_control_id);
	void _on_focus_exited(int p_tag, ObjectID p_control_id);
	void _set_focused_tag(int p_tag);
	bool _layout_and_mount(uint64_t p_revision);
	Control *_build_node(const Ref<RNShadowNode> &p_node, Control *p_parent, RNRegistry &r_registry, bool p_branch_targetable = true);
	void _queue_layout_events(const Ref<RNShadowNode> &p_node, HashMap<int, Rect2> &r_next_cache, Vector<RNNativeEvent> &r_events);
	void _apply_declarative_overrides(const Ref<RNShadowNode> &p_node);
	void _build_snapshot_node(const Ref<RNShadowNode> &p_node, int p_parent_tag, const Point2 &p_parent_position, const Transform2D &p_window_transform, const RNRegistry &p_registry, RNSurfaceSnapshot &r_snapshot);
	void _publish_transform_snapshot();
	void _route_input(const Ref<InputEvent> &p_event);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void input(const Ref<InputEvent> &p_event) override;

public:
	ReactNativeRootView();
	~ReactNativeRootView() override;

	void set_application_key(const String &p_key);
	String get_application_key() const { return application_key; }
	int get_root_tag() const { return root_tag; }
	void reload();

	void mount(const Ref<RNShadowNode> &p_tree);
	bool get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const;

	void _attach_surface(const RNSurfaceRoute &p_route);
	Vector<RNNativeEvent> _prepare_surface_stop();
	void _detach_surface(int p_root_tag, uint64_t p_epoch);
	void _accept_commit(const RNPendingCommit &p_commit);
	void _apply_imperative(const RNImperativeRequest &p_request);
};
