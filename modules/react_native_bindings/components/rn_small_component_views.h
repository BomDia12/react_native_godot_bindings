#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_small_controls.h"

class RNSwitchComponentView : public RNComponentView {
	RNSwitchControl *toggle;

public:
	explicit RNSwitchComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
};

class RNActivityIndicatorComponentView : public RNComponentView {
	RNActivityIndicatorControl *indicator;

public:
	explicit RNActivityIndicatorComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
};
