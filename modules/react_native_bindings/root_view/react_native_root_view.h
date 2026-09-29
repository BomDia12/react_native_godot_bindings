#pragma once

#include "../input/rn_input_router.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "scene/gui/control.h"

#include <memory>

class RNMountingManager;

class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	friend class RNMountingManager;

	std::unique_ptr<RNMountingManager> mounting_manager;
	RNInputRouter input_router;
	String application_key = "GodotApp";
	int root_tag = 0;
	uint64_t runtime_generation = 0;
	uint64_t surface_epoch = 0;
	uint64_t mounted_revision = 0;
	int focused_tag = 0;
	bool registered = false;
	bool transaction_in_flight = false;

	void _clear_scene_state(bool p_keep_container = false);
	void _enqueue_events(Vector<RNNativeEvent> p_events);
	void _stamp_events(Vector<RNNativeEvent> &r_events) const;
	void _on_focus_entered(int p_tag, ObjectID p_control_id);
	void _on_focus_exited(int p_tag, ObjectID p_control_id);
	void _set_focused_tag(int p_tag, const RNSurfaceSnapshot *p_old_snapshot = nullptr);
	void _publish_mounted_result(Vector<RNNativeEvent> p_events, const std::shared_ptr<const RNSurfaceSnapshot> &p_old_snapshot = nullptr);
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
	void set_mount_failure_injection(int p_before_mutation, int p_after_mutation);

	void mount(const Ref<RNShadowNode> &p_tree);
	bool get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const;

	void _attach_surface(const RNSurfaceRoute &p_route);
	Vector<RNNativeEvent> _prepare_surface_stop();
	void _detach_surface(int p_root_tag, uint64_t p_epoch);
	void _accept_commit(const RNPendingCommit &p_commit);
	bool _apply_imperative(const RNImperativeRequest &p_request);
	void _flush_imperative_updates();
};
