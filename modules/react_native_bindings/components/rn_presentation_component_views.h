#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_presentation_control.h"

class RNModalComponentView : public RNComponentView {
	RNModalControl *modal;
	void _report_screen_size();

protected:
	Control *get_child_container() override { return modal->get_overlay(); }

public:
	explicit RNModalComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_state(const facebook::react::State::Shared &p_state) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
};

class RNWindowComponentView : public RNComponentView {
	RNWindowControl *window;

protected:
	Control *get_child_container() override { return window->get_content(); }

public:
	explicit RNWindowComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
};
