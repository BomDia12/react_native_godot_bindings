#include "rn_example_meter.h"

#include "../fabric/rn_shadow_node.h"
#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"

#include "core/object/callable_mp.h"
#include "scene/gui/progress_bar.h"

#include <cmath>

namespace {

constexpr const char *VALUE_REVISION_META = "rn_declarative_value_revision";
constexpr const char *COMMAND_OVERRIDE_META = "rn_command_override";

Dictionary meter_view_config() {
	Dictionary native_props;
	native_props["value"] = "double";
	native_props["tint"] = "Color";
	Dictionary direct;
	Dictionary changed;
	changed["registrationName"] = "onValueChanged";
	direct["topValueChanged"] = changed;
	Dictionary config;
	config["uiViewClassName"] = "RNExampleMeter";
	config["NativeProps"] = native_props;
	config["directEventTypes"] = direct;
	config["bubblingEventTypes"] = Dictionary();
	return config;
}

class RNExampleMeterDescriptor : public RNHostDescriptor {
public:
	RNExampleMeterDescriptor() :
			RNHostDescriptor("RNExampleMeter", RNHostTraits{ true, true, false, true, false, false, false, false, false, true, true }, meter_view_config()) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		const Variant value = p_node.props.get("value", 0.0);
		if ((value.get_type() != Variant::FLOAT && value.get_type() != Variant::INT) || !std::isfinite(double(value)) || double(value) < 0 || double(value) > 1) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "value must be a finite number in 0-1", "RNExampleMeter.prepare", "props.value");
			return false;
		}
		if (p_node.props.has("tint")) {
			Color tint;
			if (!RNViewStyle::color_of(p_node.props, "tint", tint)) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "tint must be a Godot Color wrapper", "RNExampleMeter.prepare", "props.tint");
				return false;
			}
			r_state.props["tint"] = tint;
		}
		return true;
	}

	Control *create_host(const RNHostContext &p_context) const override {
		(void)p_context;
		ProgressBar *meter = memnew(ProgressBar);
		meter->set_min(0);
		meter->set_max(1);
		meter->set_step(0.001);
		meter->set_show_percentage(false);
		return meter;
	}

	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context, RNError &r_error) const override {
		(void)p_context;
		ProgressBar *meter = Object::cast_to<ProgressBar>(p_host);
		if (!meter) {
			r_error = RNError::make(RNErrorCode::NATIVE, "RNExampleMeter host is not a ProgressBar", "RNExampleMeter.apply");
			return false;
		}
		meter->set_visible(String(p_state.props.get("display", "flex")) != "none");
		const uint64_t *revision = p_state.declarative_prop_revisions.getptr("value");
		const int64_t desired_revision = revision ? int64_t(*revision) : 0;
		if (!meter->has_meta(VALUE_REVISION_META) || int64_t(meter->get_meta(VALUE_REVISION_META)) != desired_revision) {
			meter->set_value_no_signal(double(p_state.props.get("value", 0.0)));
			meter->set_meta(VALUE_REVISION_META, desired_revision);
			meter->set_meta(COMMAND_OVERRIDE_META, false);
		}
		meter->set_modulate(p_state.props.get("tint", Color(1, 1, 1, 1)));
		return true;
	}

	Variant capture_state(Control *p_host) const override {
		ProgressBar *meter = Object::cast_to<ProgressBar>(p_host);
		if (!meter) {
			return Variant();
		}
		Dictionary state;
		state["value"] = meter->get_value();
		state["modulate"] = meter->get_modulate();
		state["visible"] = meter->is_visible();
		state["revision"] = meter->get_meta(VALUE_REVISION_META, int64_t(0));
		state["override"] = meter->get_meta(COMMAND_OVERRIDE_META, false);
		return state;
	}

	void restore_state(Control *p_host, const Variant &p_state) const override {
		ProgressBar *meter = Object::cast_to<ProgressBar>(p_host);
		if (!meter || p_state.get_type() != Variant::DICTIONARY) {
			return;
		}
		const Dictionary state = p_state;
		meter->set_value_no_signal(state.get("value", 0.0));
		meter->set_modulate(state.get("modulate", Color(1, 1, 1, 1)));
		meter->set_visible(state.get("visible", true));
		meter->set_meta(VALUE_REVISION_META, state.get("revision", int64_t(0)));
		meter->set_meta(COMMAND_OVERRIDE_META, state.get("override", false));
	}

	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &p_constraints) const override {
		(void)p_state;
		Size2 result(80, 20);
		if (p_constraints.width_mode == RNMeasureMode::EXACTLY) {
			result.x = p_constraints.width;
		} else if (p_constraints.width_mode == RNMeasureMode::AT_MOST) {
			result.x = MIN(result.x, p_constraints.width);
		}
		if (p_constraints.height_mode == RNMeasureMode::EXACTLY) {
			result.y = p_constraints.height;
		} else if (p_constraints.height_mode == RNMeasureMode::AT_MOST) {
			result.y = MIN(result.y, p_constraints.height);
		}
		return result;
	}

	void attach_signals(Control *p_host, const RNHostContext &p_context) const override {
		if (!p_context.owner) {
			return;
		}
		const Callable callback = callable_mp(p_context.owner, &ReactNativeRootView::_on_descriptor_value_changed).bind(p_context.tag, p_host->get_instance_id());
		if (!p_host->is_connected("value_changed", callback)) {
			p_host->connect("value_changed", callback);
		}
	}

	void detach_signals(Control *p_host, const RNHostContext &p_context) const override {
		if (!p_context.owner) {
			return;
		}
		const Callable callback = callable_mp(p_context.owner, &ReactNativeRootView::_on_descriptor_value_changed).bind(p_context.tag, p_host->get_instance_id());
		if (p_host->is_connected("value_changed", callback)) {
			p_host->disconnect("value_changed", callback);
		}
	}

	bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const override {
		if (p_command != "advance") {
			return RNHostDescriptor::dispatch_command(p_host, p_command, p_arguments, p_context, r_error);
		}
		ProgressBar *meter = Object::cast_to<ProgressBar>(p_host);
		Variant amount = p_arguments;
		if (p_arguments.get_type() == Variant::ARRAY && !Array(p_arguments).is_empty()) {
			amount = Array(p_arguments)[0];
		}
		if (!meter || (amount.get_type() != Variant::FLOAT && amount.get_type() != Variant::INT) || !std::isfinite(double(amount))) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "advance expects one finite increment", "RNExampleMeter.advance", "args.amount");
			return false;
		}
		meter->set_meta(COMMAND_OVERRIDE_META, true);
		meter->set_value(CLAMP(meter->get_value() + double(amount), 0.0, 1.0));
		return true;
	}
};

} // namespace

bool rn_register_example_meter(RNHostDescriptorRegistry &p_registry, RNError &r_error) {
	return p_registry.register_descriptor(std::make_shared<RNExampleMeterDescriptor>(), r_error);
}
