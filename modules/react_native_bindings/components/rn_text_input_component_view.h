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
	~RNTextInputComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
	RNTextInputControl *get_input_control() const { return input; }
	Vector<Control *> get_input_controls() const override { return { input->get_editor() }; }
	bool owns_input_control(Control *p_control) const override { return p_control == input || p_control == input->get_editor() || input->get_editor()->is_ancestor_of(p_control); }
	Control *get_focus_control() const override { return input->get_editor(); }
	bool owns_activation() const override { return true; }
};
