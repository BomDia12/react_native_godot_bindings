#pragma once

#include "../input/rn_input_router.h"
#include "../interop/rn_scene_binding.h"
#include "../runtime/react_native_runtime_coordinator.h"

#include "scene/gui/control.h"

#include <memory>

class RNMountingManager;

class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	friend class RNMountingManager;

	std::shared_ptr<const RNSceneAttachment> scene_attachment;
	uint64_t next_binding_identity = 1;
	bool scene_target_available = false;
	void _disconnect_scene_target();
	void _scene_target_changed(bool p_available);
	Callable alert_handler;
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
	Vector<RNNativeEvent> descriptor_events;
	uint64_t native_resource_revision = 1;
	bool native_layout_pending = false;

	void _clear_scene_state(bool p_keep_container = false);
	void _enqueue_events(Vector<RNNativeEvent> p_events);
	void _stamp_events(Vector<RNNativeEvent> &r_events) const;
	void _set_focused_tag(int p_tag, const RNSurfaceSnapshot *p_old_snapshot = nullptr);
	void _publish_mounted_result(Vector<RNNativeEvent> p_events, const std::shared_ptr<const RNSurfaceSnapshot> &p_old_snapshot = nullptr);
	void _publish_transform_snapshot();
	void _route_input(const Ref<InputEvent> &p_event, int p_native_tag = 0, Viewport *p_viewport = nullptr);
	void _refresh_native_dependencies();

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
	void set_alert_handler(const Callable &p_handler) { alert_handler = p_handler; }
	Callable get_alert_handler() const { return alert_handler; }
	bool complete_alert(const String &p_request, const Dictionary &p_result);
	Dictionary attach_scene_binding(Object *p_target, const Ref<RNSceneBinding> &p_binding);
	void detach_scene_binding();
	std::shared_ptr<const RNSceneAttachment> get_scene_attachment() const { return scene_target_available ? scene_attachment : nullptr; }
	void set_mount_failure_injection(int p_before_mutation, int p_after_mutation);

	void mount(const Ref<RNShadowNode> &p_tree);
	bool get_measurement(int p_tag, Rect2 &r_local_rect, Point2 &r_page_position) const;

	void _attach_surface(const RNSurfaceRoute &p_route);
	Vector<RNNativeEvent> _prepare_surface_stop();
	void _detach_surface(int p_root_tag, uint64_t p_epoch);
	void _accept_commit(const RNPendingCommit &p_commit);
	bool _apply_imperative(const RNImperativeRequest &p_request);
	void _flush_imperative_updates();
	void _on_focus_entered(int p_tag, ObjectID p_control_id);
	void _on_focus_exited(int p_tag, ObjectID p_control_id);
	void _cancel_host_input();
	void _on_gui_input_dispatched(const Ref<InputEvent> &p_event, uint64_t p_control_id, uint64_t p_event_id);
	void _on_descriptor_value_changed(double p_value, int p_tag, ObjectID p_control_id);
	void _emit_host_event(uint64_t p_generation, uint64_t p_epoch, int p_tag, ObjectID p_host, const StringName &p_name, const Dictionary &p_payload, uint64_t p_revision);
	void _invalidate_host_geometry();
	void _invalidate_host_layout();
	uint64_t get_native_resource_revision() const { return native_resource_revision; }
};
