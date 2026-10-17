#pragma once

#include "GodotTextInputShadowNode.h"
#include "components/rn_text_shaper.h"

#include <react/renderer/core/ConcreteComponentDescriptor.h>

namespace facebook::react {

class GodotTextInputComponentDescriptor final : public ConcreteComponentDescriptor<GodotTextInputShadowNode> {
public:
	explicit GodotTextInputComponentDescriptor(const ComponentDescriptorParameters &parameters) :
			ConcreteComponentDescriptor<GodotTextInputShadowNode>(parameters),
			textLayoutManager_(std::make_shared<TextLayoutManager>(contextContainer_)),
			shaper_(contextContainer_->at<std::shared_ptr<const RNTextShaper>>(RNTextShaper::CONTEXT_KEY)) {}

protected:
	void adopt(ShadowNode &shadowNode) const override {
		auto &textInput = static_cast<GodotTextInputShadowNode &>(shadowNode);
		textInput.setTextLayoutManager(textLayoutManager_);
		textInput.setTextShaper(shaper_);
		ConcreteComponentDescriptor::adopt(shadowNode);
	}

private:
	const std::shared_ptr<const TextLayoutManager> textLayoutManager_;
	const std::shared_ptr<const RNTextShaper> shaper_;
};

} // namespace facebook::react
