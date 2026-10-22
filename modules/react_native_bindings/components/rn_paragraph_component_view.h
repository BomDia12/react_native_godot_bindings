#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_style_box.h"
#include "rn_text_control.h"

#include <react/renderer/attributedstring/AttributedString.h>
#include <react/renderer/attributedstring/ParagraphAttributes.h>

class RNTextShaper;

class RNParagraphComponentView : public RNComponentView {
	RNTextControl *text;
	std::shared_ptr<const RNTextShaper> shaper;
	facebook::react::AttributedString attributed;
	facebook::react::ParagraphAttributes paragraph_attributes;
	Ref<RNStyleBox> box;
	void _apply_box();
	void _set_content();
	void _follow_text_colors(const facebook::react::TextAttributes &p_before, const facebook::react::TextAttributes &p_after);

public:
	RNParagraphComponentView(facebook::react::Tag p_tag, std::shared_ptr<const RNTextShaper> p_shaper);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_state(const facebook::react::State::Shared &p_state) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
	RNTextControl *get_text_control() const { return text; }
	RNTouchTarget touch_target_at(const Point2 &p_local) const override;
	bool owns_activation() const override { return text->is_selectable(); }
};
