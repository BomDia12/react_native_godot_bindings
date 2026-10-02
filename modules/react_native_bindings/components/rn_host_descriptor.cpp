#include "rn_host_descriptor.h"

#include "../fabric/rn_shadow_node.h"

RNHostDescriptor::RNHostDescriptor(const StringName &p_name, const RNHostTraits &p_traits, const Dictionary &p_view_config) :
		name(p_name),
		traits(p_traits),
		view_config(p_view_config.duplicate(true)) {
}

bool RNHostDescriptor::prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const {
	(void)r_error;
	r_state.props = p_node.props.duplicate(true);
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
