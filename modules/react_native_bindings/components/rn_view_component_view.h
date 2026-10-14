#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_view_control.h"

class RNViewComponentView : public RNComponentView {
	RNViewControl *view;
	void _apply_style();

public:
	explicit RNViewComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
};
