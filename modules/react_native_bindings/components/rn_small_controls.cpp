#include "rn_small_controls.h"

#include "../fabric/rn_view_style.h"

#include "core/object/callable_mp.h"

namespace {
Dictionary switch_config() {
	Dictionary registration;
	registration["registrationName"] = "onChange";
	Dictionary events;
	events["topChange"] = registration;
	Dictionary config;
	config["directEventTypes"] = events;
	return config;
}

class RNSwitchDescriptor : public RNHostDescriptor {
public:
	RNSwitchDescriptor() :
			RNHostDescriptor("RCTSwitch", RNHostTraits{ true, true, false, true, false, false, true, true, false, true, true }, switch_config()) {}
	Control *create_host(const RNHostContext &) const override { return memnew(RNSwitchControl); }
	bool owns_native_activation(const RNPreparedHostState &) const override { return true; }
	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		auto *control = Object::cast_to<RNSwitchControl>(p_host);
		control->set_pressed_no_signal(p_state.props.get("value", false));
		control->set_disabled(p_state.props.get("disabled", false));
		const String pointer_events = p_state.props.get("pointerEvents", "auto");
		const bool targetable = p_state.branch_targetable && pointer_events != "none" && pointer_events != "box-none";
		control->set_mouse_filter(targetable ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_IGNORE);
		control->set_visible(String(p_state.props.get("display", "flex")) != "none");
		control->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
		Color tint;
		control->set_self_modulate(RNViewStyle::color_of(p_state.props, "thumbTintColor", tint) ? tint : Color(1, 1, 1));
		return true;
	}
	Size2 measure(const RNPreparedHostState &, const RNMeasureConstraints &) const override { return Size2(48, 28); }
	Variant capture_state(Control *p_host) const override {
		auto *control = Object::cast_to<RNSwitchControl>(p_host);
		Dictionary state;
		state["value"] = control->is_pressed();
		state["disabled"] = control->is_disabled();
		state["mouse_filter"] = int(control->get_mouse_filter());
		state["visible"] = control->is_visible();
		state["modulate"] = control->get_modulate();
		state["tint"] = control->get_self_modulate();
		return state;
	}
	void restore_state(Control *p_host, const Variant &p_state) const override {
		auto *control = Object::cast_to<RNSwitchControl>(p_host);
		Dictionary state = p_state;
		control->set_pressed_no_signal(state["value"]);
		control->set_disabled(state["disabled"]);
		control->set_mouse_filter(Control::MouseFilter(int(state["mouse_filter"])));
		control->set_visible(state["visible"]);
		control->set_modulate(state["modulate"]);
		control->set_self_modulate(state["tint"]);
	}
	void after_publish(Control *p_host, const RNPreparedHostState &, const RNHostContext &p_context) const override { Object::cast_to<RNSwitchControl>(p_host)->publish(p_context); }
	bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const override {
		Array args = p_arguments;
		if (p_command == "setValue" && args.size() == 1 && args[0].get_type() == Variant::BOOL) {
			Object::cast_to<RNSwitchControl>(p_host)->set_pressed_no_signal(args[0]);
			return true;
		}
		return RNHostDescriptor::dispatch_command(p_host, p_command, p_arguments, p_context, r_error);
	}
};

class RNActivityDescriptor : public RNHostDescriptor {
public:
	RNActivityDescriptor() :
			RNHostDescriptor("RCTActivityIndicatorView", RNHostTraits{ true, true, false, true, false, false, false, false, false, true, true }) {}
	Control *create_host(const RNHostContext &) const override { return memnew(RNActivityIndicatorControl); }
	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		auto *control = Object::cast_to<RNActivityIndicatorControl>(p_host);
		bool animating = p_state.props.get("animating", true);
		control->set_indeterminate(animating);
		control->set_visible(String(p_state.props.get("display", "flex")) != "none" && (animating || !bool(p_state.props.get("hidesWhenStopped", true))));
		Color tint;
		if (!RNViewStyle::color_of(p_state.props, "color", tint)) {
			tint = Color(1, 1, 1);
		}
		tint.a *= RNViewStyle::opacity_of(p_state.props);
		control->set_modulate(tint);
		return true;
	}
	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &) const override { return String(p_state.props.get("size", "small")) == "large" ? Size2(36, 36) : Size2(20, 20); }
	Variant capture_state(Control *p_host) const override {
		auto *control = Object::cast_to<RNActivityIndicatorControl>(p_host);
		Dictionary state;
		state["animating"] = control->is_indeterminate();
		state["visible"] = control->is_visible();
		state["modulate"] = control->get_modulate();
		return state;
	}
	void restore_state(Control *p_host, const Variant &p_state) const override {
		auto *control = Object::cast_to<RNActivityIndicatorControl>(p_host);
		Dictionary state = p_state;
		control->set_indeterminate(state["animating"]);
		control->set_visible(state["visible"]);
		control->set_modulate(state["modulate"]);
	}
};
} //namespace

RNSwitchControl::RNSwitchControl() {
	connect("toggled", callable_mp(this, &RNSwitchControl::_changed));
}
void RNSwitchControl::_changed(bool p_value) {
	if (published_context.event_sink && published_context.event_sink->emit) {
		Dictionary event;
		event["value"] = p_value;
		published_context.event_sink->emit(published_context.tag, "topChange", event, published_context.revision);
	}
}
RNActivityIndicatorControl::RNActivityIndicatorControl() {
	set_show_percentage(false);
	set_indeterminate(true);
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}
std::shared_ptr<const RNHostDescriptor> rn_switch_descriptor() {
	return std::make_shared<RNSwitchDescriptor>();
}
std::shared_ptr<const RNHostDescriptor> rn_activity_indicator_descriptor() {
	return std::make_shared<RNActivityDescriptor>();
}
