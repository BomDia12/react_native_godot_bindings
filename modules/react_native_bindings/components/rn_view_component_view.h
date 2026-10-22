#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_clip.h"
#include "rn_effect_host.h"
#include "rn_view_control.h"

class RNViewComponentView : public RNComponentView {
	RNViewControl *view;
	RNEffectHost *effect = nullptr;
	RNClipControl *clip = nullptr;
	bool reported_drop_shadows = false;
	void _apply_style();
	void _restructure(bool p_effect, bool p_clip);

protected:
	Control *get_child_container() override;

public:
	explicit RNViewComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
	RNEffectHost *get_effect_host() const { return effect; }
	RNClipControl *get_clip_host() const { return clip; }
};

RNAccessibility rn_view_accessibility(const facebook::react::ViewProps &p_props);
RNStyleBox::Paint rn_view_paint(const facebook::react::ViewProps &p_props, const facebook::react::LayoutMetrics &p_layout);
