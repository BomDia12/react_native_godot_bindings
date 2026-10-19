#pragma once

#include "../input/rn_input_router.h"
#include "../interop/rn_scene_binding.h"
#include "../mounting/rn_mounting_manager.h"

#include "scene/gui/control.h"

#include <memory>

// Hosts one React Native surface. The surface restarts with a new id whenever
// the root re-enters the tree, reloads or changes its application key.
class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

	String application_key = "GodotApp";
	facebook::react::SurfaceId surface_id = 0;
	Control *container = nullptr;
	std::unique_ptr<RNRootComponentView> root_component;
	std::unique_ptr<RNComponentViewRegistry> views;
	RNInputRouter input_router;
	uint64_t input_serial = 1;
	uint64_t claimed_input_serial = 0;
	Callable alert_handler;
	std::shared_ptr<const RNSceneAttachment> scene_attachment;
	uint64_t next_binding_identity = 1;
	bool scene_target_available = false;
	bool layout_pending = false;

	void _connect_input(RNComponentView *p_view);
	void _connect_control(Control *p_control);
	void _route_unclaimed_motion(const Ref<InputEvent> &p_event, uint64_t p_serial, ObjectID p_viewport_id);
	void _route(const Ref<InputEvent> &p_event, RNComponentView *p_view, Viewport *p_viewport);
	RNComponentView *_view_inside_clips(RNComponentView *p_view, const Point2 &p_canvas_point) const;
	void _cancel_mouse_taken_by_popup(ObjectID p_viewport_id);
	Control *_focusable_at(Control *p_pressed, const Point2 &p_viewport_position) const;
	void _disconnect_scene_target();
	void _scene_target_changed(bool p_available);
	void _surface_changed();
	void _update_layout();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void input(const Ref<InputEvent> &p_event) override;

public:
	ReactNativeRootView();
	~ReactNativeRootView() override;

	void set_application_key(const String &p_key);
	String get_application_key() const { return application_key; }
	int get_root_tag() const { return surface_id; }
	void reload();
	void set_alert_handler(const Callable &p_handler) { alert_handler = p_handler; }
	Callable get_alert_handler() const { return alert_handler; }
	bool complete_alert(const String &p_request, const Dictionary &p_result);
	Dictionary attach_scene_binding(Object *p_target, const Ref<RNSceneBinding> &p_binding);
	void detach_scene_binding();
	std::shared_ptr<const RNSceneAttachment> get_scene_attachment() const { return scene_target_available ? scene_attachment : nullptr; }

	RNMountedSurface _attach_surface(facebook::react::SurfaceId p_surface, std::shared_ptr<const RNComponentViewFactories> p_factories);
	void _detach_surface();
	void _begin_input(const Ref<InputEvent> &p_event, ObjectID p_viewport_id);
	void _on_host_gui_input(const Ref<InputEvent> &p_event, ObjectID p_control_id);
};
