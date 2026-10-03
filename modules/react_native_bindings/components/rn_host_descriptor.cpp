#include "rn_host_descriptor.h"

#include "../fabric/rn_shadow_node.h"
#include "rn_visual_style.h"

#include "scene/main/viewport.h"

RNHostDescriptor::RNHostDescriptor(const StringName &p_name, const RNHostTraits &p_traits, const Dictionary &p_view_config) :
		name(p_name),
		traits(p_traits),
		view_config(p_view_config.duplicate(true)) {
}

bool RNHostDescriptor::prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const {
	r_state.props = p_node.props.duplicate(true);
	const Array keys = r_state.props.keys();
	for (const Variant &key : keys) {
		if (r_state.props[key].get_type() == Variant::NIL) {
			r_state.props.erase(key);
		}
	}
	if (!RNVisualStyle::validate(r_state.props, r_error)) {
		return false;
	}
	r_state.declarative_prop_revisions = p_node.declarative_prop_revisions;
	if (traits.collects_text) {
		r_state.text = p_node.collect_text();
	}
	return true;
}

Control *RNHostDescriptor::create_host(const RNHostContext &p_context) const {
	(void)p_context;
	return nullptr;
}

bool RNHostDescriptor::resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &r_error) const {
	(void)r_state;
	(void)p_context;
	(void)r_error;
	return true;
}

bool RNHostDescriptor::apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context, RNError &r_error) const {
	(void)p_host;
	(void)p_state;
	(void)p_context;
	r_error = RNError::make(RNErrorCode::NATIVE, vformat("descriptor '%s' does not create a host", name), "descriptor.apply");
	return false;
}

Variant RNHostDescriptor::capture_state(Control *p_host) const {
	(void)p_host;
	return Variant();
}

void RNHostDescriptor::restore_state(Control *p_host, const Variant &p_state) const {
	(void)p_host;
	(void)p_state;
}

Size2 RNHostDescriptor::measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &p_constraints) const {
	(void)p_state;
	(void)p_constraints;
	return Size2();
}

float RNHostDescriptor::baseline(const RNPreparedHostState &p_state, const Size2 &p_size) const {
	(void)p_state;
	return p_size.y;
}

Control *RNHostDescriptor::get_child_container(Control *p_host, const RNHostContext &p_context) const {
	(void)p_context;
	return p_host;
}

RNChildLayoutPolicy RNHostDescriptor::get_child_layout_policy(const RNPreparedHostState &p_state) const {
	(void)p_state;
	return traits.measured_leaf ? RNChildLayoutPolicy::MEASURED_LEAF : RNChildLayoutPolicy::ORDINARY;
}

void RNHostDescriptor::after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const {
	(void)p_host;
	(void)p_state;
	(void)p_context;
}

RNHostGeometry RNHostDescriptor::read_geometry(Control *p_host, const RNHostContext &p_context) const {
	(void)p_context;
	RNHostGeometry geometry;
	if (p_host) {
		geometry.transform = p_host->is_inside_tree() ? p_host->get_global_transform_with_canvas() : p_host->get_transform();
		geometry.screen_transform = p_host->is_inside_tree() ? p_host->get_screen_transform() : geometry.transform;
		geometry.viewport = Rect2(Point2(), p_host->get_size());
		geometry.bounds = geometry.viewport;
		geometry.content_size = p_host->get_size();
		geometry.clips_contents = p_host->is_clipping_contents();
		if (Viewport *viewport = p_host->is_inside_tree() ? p_host->get_viewport() : nullptr) {
			geometry.viewport_id = viewport->get_instance_id();
		}
	}
	return geometry;
}

void RNHostDescriptor::attach_signals(Control *p_host, const RNHostContext &p_context) const {
	(void)p_host;
	(void)p_context;
}

void RNHostDescriptor::detach_signals(Control *p_host, const RNHostContext &p_context) const {
	(void)p_host;
	(void)p_context;
}

void RNHostDescriptor::dispose_state(Control *p_host, const RNHostContext &p_context) const {
	(void)p_host;
	(void)p_context;
}

bool RNHostDescriptor::dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const {
	(void)p_host;
	(void)p_arguments;
	(void)p_context;
	r_error = RNError::make(RNErrorCode::UNSUPPORTED, vformat("command '%s' is not supported by '%s'", p_command, name), "descriptor.command");
	return false;
}

bool RNHostDescriptor::dispatch_accessibility_action(Control *p_host, const StringName &p_action, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const {
	(void)p_host;
	(void)p_arguments;
	(void)p_context;
	r_error = RNError::make(RNErrorCode::UNSUPPORTED, vformat("accessibility action '%s' is pending for '%s'", p_action, name), "descriptor.accessibility");
	return false;
}

Rect2 RNHostDescriptor::get_child_layout_viewport(const RNPreparedHostState &, const Size2 &p_size) const {
	return Rect2(Point2(), p_size);
}

bool RNHostDescriptor::owns_input_control(Control *p_host, Control *p_control) const {
	return p_host == p_control;
}

Control *RNHostDescriptor::focus_control(Control *p_host) const {
	return p_host;
}
bool RNHostDescriptor::owns_native_activation(const RNPreparedHostState &) const {
	return false;
}

Size2 RNHostDescriptor::presentation_size(const RNPreparedHostState &, const Size2 &p_inherited) const {
	return p_inherited;
}
