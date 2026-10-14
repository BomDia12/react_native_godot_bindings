#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_example_meter.h"

class RNExampleMeterComponentView : public RNComponentView {
	RNExampleMeterControl *meter;
	std::function<void(double)> value_changed;
	void _value_changed(double p_value);

public:
	explicit RNExampleMeterComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
};
