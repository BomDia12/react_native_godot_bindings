#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_scroll_control.h"

class RNScrollComponentView : public RNComponentView {
	RNScrollControl *scroll;
	void _emit(const StringName &p_event);

protected:
	Control *get_child_container() override { return scroll->get_content(); }

public:
	explicit RNScrollComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_state(const facebook::react::State::Shared &p_state) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
	RNScrollControl *get_scroll_control() const { return scroll; }
};
