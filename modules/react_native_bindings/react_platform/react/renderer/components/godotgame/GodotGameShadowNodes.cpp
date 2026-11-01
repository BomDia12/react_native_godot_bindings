#include "GodotGameShadowNodes.h"

namespace facebook::react {

extern const char GodotProgressBarComponentName[] = "GodotProgressBar";
extern const char GodotSliderComponentName[] = "RNCSlider";
extern const char GodotRichTextComponentName[] = "GodotRichText";

GodotRichTextShadowNode::GodotRichTextShadowNode(const ShadowNode &source, const ShadowNodeFragment &fragment) :
		ConcreteViewShadowNode(source, fragment) {
	const auto &previous = static_cast<const GodotRichTextProps &>(*source.getProps());
	const GodotRichTextProps &next = getConcreteProps();
	if (previous.bbcode != next.bbcode || previous.godotThemeType != next.godotThemeType) {
		dirtyLayout();
	}
}

Size GodotRichTextShadowNode::measureContent(const LayoutContext &, const LayoutConstraints &layoutConstraints) const {
	if (!theme_ || !measurer_) {
		return layoutConstraints.clamp({ .width = 0, .height = 0 });
	}
	const GodotRichTextProps &props = getConcreteProps();
	const std::shared_ptr<const RNThemeMetrics> metrics = theme_->get(getSurfaceId());
	const Size2 size = measurer_->measure(String::utf8(props.bbcode.c_str()), props.godotThemeType, metrics, layoutConstraints.maximumSize.width);
	return layoutConstraints.clamp({ .width = size.x, .height = size.y });
}

} // namespace facebook::react
