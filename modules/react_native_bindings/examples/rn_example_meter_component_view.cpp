#include "rn_example_meter_component_view.h"

#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>

using namespace facebook::react;

RNExampleMeterComponentView::RNExampleMeterComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNExampleMeterControl)), meter(Object::cast_to<RNExampleMeterControl>(get_control())) {
	meter->set_min(0);
	meter->set_max(1);
	meter->set_step(0.001);
	meter->set_show_percentage(false);
	meter->set_value_changed_callback([this](double p_value) { _value_changed(p_value); });
}

void RNExampleMeterComponentView::_value_changed(double p_value) {
	if (value_changed) {
		value_changed(p_value);
	}
}

void RNExampleMeterComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const RNExampleMeterProps &>(*p_new);
	const auto previous = std::static_pointer_cast<const RNExampleMeterProps>(p_old);
	if (!previous || previous->value != next.value) {
		meter->set_value_no_signal(next.value);
	}
	::Color tint = rn_godot_color(next.tint, ::Color(1, 1, 1));
	tint.a *= next.opacity;
	meter->set_modulate(tint);
}

void RNExampleMeterComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const RNExampleMeterEventEmitter>(p_emitter);
	value_changed = emitter ? std::function<void(double)>([emitter](double p_value) { emitter->onValueChanged({ .value = p_value }); }) : nullptr;
}

void RNExampleMeterComponentView::handle_command(const std::string &p_name, const folly::dynamic &p_args) {
	if (p_name == "advance" && p_args.size() == 1 && p_args[0].isNumber()) {
		meter->set_value(CLAMP(meter->get_value() + p_args[0].asDouble(), 0.0, 1.0));
	} else {
		ERR_PRINT(vformat("RNExampleMeter does not support the command '%s'.", String::utf8(p_name.c_str())));
	}
}
