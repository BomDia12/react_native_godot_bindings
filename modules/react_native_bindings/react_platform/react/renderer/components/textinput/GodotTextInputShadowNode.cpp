#include "GodotTextInputShadowNode.h"

#include "components/rn_text_shaper.h"

namespace facebook::react {

extern const char GodotTextInputComponentName[] = "GodotTextInput";

void GodotTextInputShadowNode::setTextShaper(std::shared_ptr<const RNTextShaper> shaper) {
	ensureUnsealed();
	shaper_ = std::move(shaper);
}

// The Godot editor draws its theme stylebox around the text, so its content
// margins add to the measured text like Android's theme padding does.
Size GodotTextInputShadowNode::measureContent(const LayoutContext &layoutContext, const LayoutConstraints &layoutConstraints) const {
	const std::shared_ptr<const RNThemeMetrics> metrics = shaper_->get_theme_metrics(getSurfaceId());
	const Rect2 margins = getConcreteProps().multiline ? metrics->text_edit_margins : metrics->line_edit_margins;
	const Size horizontal{ .width = margins.position.x + margins.size.x, .height = margins.position.y + margins.size.y };
	LayoutConstraints textConstraints = layoutConstraints;
	textConstraints.minimumSize = { .width = std::max<Float>(0, layoutConstraints.minimumSize.width - horizontal.width), .height = std::max<Float>(0, layoutConstraints.minimumSize.height - horizontal.height) };
	textConstraints.maximumSize = { .width = layoutConstraints.maximumSize.width - horizontal.width, .height = layoutConstraints.maximumSize.height - horizontal.height };
	const Size text = BaseTextInputShadowNode::measureContent(layoutContext, textConstraints);
	return layoutConstraints.clamp({ .width = text.width + horizontal.width, .height = text.height + horizontal.height });
}

} // namespace facebook::react
