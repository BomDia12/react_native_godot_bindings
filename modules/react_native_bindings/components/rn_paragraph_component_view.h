#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_text_control.h"

#include <react/renderer/attributedstring/AttributedString.h>

class RNTextShaper;

class RNParagraphComponentView : public RNComponentView {
	RNTextControl *text;
	std::shared_ptr<const RNTextShaper> shaper;
	facebook::react::AttributedString attributed;

public:
	RNParagraphComponentView(facebook::react::Tag p_tag, std::shared_ptr<const RNTextShaper> p_shaper);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_state(const facebook::react::State::Shared &p_state) override;
	RNTextControl *get_text_control() const { return text; }
	RNTouchTarget touch_target_at(const Point2 &p_local) const override;
	bool owns_activation() const override { return text->is_selectable(); }
};
