#pragma once

#include "../input/rn_input_router.h"
#include "../interop/rn_component_contract.h"
#include "../interop/rn_scene_binding.h"
#include "../mounting/rn_mounting_manager.h"

#include "scene/gui/control.h"

#include <memory>

// What JavaScript sees of a root through useGodotRootState().
struct RNRootState {
	bool visible = false;
	bool processing = false;
	bool focused = false;

	bool operator==(const RNRootState &) const = default;
};

// Hosts one React Native surface. The surface restarts with a new id whenever
// the root re-enters the tree, reloads, or changes its application key or
// initial properties.
class ReactNativeRootView : public Control {
	GDCLASS(ReactNativeRootView, Control);

public:
	// Which axes React sizes to the content, as RCTRootViewSizeFlexibility does.
	enum FitContent {
		FIT_CONTENT_NONE,
		FIT_CONTENT_WIDTH,
		FIT_CONTENT_HEIGHT,
		FIT_CONTENT_BOTH,
	};

private:
	String application_key = "GodotApp";
	Dictionary initial_properties;
	FitContent fit_content = FIT_CONTENT_NONE;
	Size2 content_size;
	bool editor_preview = true;
	RNRootState reported_state;
	Ref<RNComponentContract> contract_resource;
	std::shared_ptr<const RNCompiledContract> contract;
	Dictionary contract_values;
	String contract_error;
	// The argument names of each event signal the contract added.
	HashMap<StringName, Vector<StringName>> contract_signals;
	bool contract_flush_pending = false;
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
	void _content_size_changed(const Size2 &p_size);
	void _state_changed();
	void _connect_window(bool p_connect);
	bool _is_running() const;
	void _draw_placeholder();
	void _load_contract();
	void _apply_contract();
	void _fit_contract_values();
	bool _contract_events_free(const RNCompiledContract &p_contract, RNError &r_error) const;
	void _contract_values_changed();
	void _flush_contract();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_value) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void input(const Ref<InputEvent> &p_event) override;

public:
	ReactNativeRootView();
	~ReactNativeRootView() override;

	void set_application_key(const String &p_key);
	String get_application_key() const { return application_key; }
	void set_initial_properties(const Dictionary &p_properties);
	Dictionary get_initial_properties() const { return initial_properties; }
	void set_fit_content(FitContent p_fit);
	FitContent get_fit_content() const { return fit_content; }
	bool fits_width() const { return fit_content == FIT_CONTENT_WIDTH || fit_content == FIT_CONTENT_BOTH; }
	bool fits_height() const { return fit_content == FIT_CONTENT_HEIGHT || fit_content == FIT_CONTENT_BOTH; }
	void set_editor_preview(bool p_preview);
	bool is_editor_preview() const { return editor_preview; }
	Size2 get_minimum_size() const override;
	RNRootState get_state() const;
	Node *get_native_node(const String &p_native_id) const;
	PackedStringArray get_configuration_warnings() const override;
	void set_contract_values(const Dictionary &p_values);
	Dictionary get_contract_values() const { return contract_values; }
	bool has_contract() const { return contract != nullptr; }
	// A contract component starts with its contract values as props.
	Dictionary get_surface_properties() const { return contract ? contract_values : initial_properties; }
	void post_message(const String &p_name, const Variant &p_payload);
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

VARIANT_ENUM_CAST(ReactNativeRootView::FitContent);
