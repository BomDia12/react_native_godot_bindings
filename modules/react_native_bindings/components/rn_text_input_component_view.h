#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_text_input_control.h"

class RNTextShaper;

class RNTextInputComponentView : public RNComponentView {
	RNTextInputControl *input;
	std::shared_ptr<const RNTextShaper> shaper;
	void _emit(const StringName &p_event, const Dictionary &p_payload);

public:
	RNTextInputComponentView(facebook::react::Tag p_tag, std::shared_ptr<const RNTextShaper> p_shaper);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
	RNTextInputControl *get_input_control() const { return input; }
};
