#pragma once

#include "GodotTextInputProps.h"

#include <react/renderer/components/textinput/BaseTextInputShadowNode.h>
#include <react/renderer/components/textinput/TextInputEventEmitter.h>
#include <react/renderer/components/textinput/TextInputState.h>

#include <memory>

class RNTextShaper;

namespace facebook::react {

extern const char GodotTextInputComponentName[];

class GodotTextInputShadowNode final : public BaseTextInputShadowNode<GodotTextInputComponentName, GodotTextInputProps, TextInputEventEmitter, TextInputState> {
public:
	using BaseTextInputShadowNode::BaseTextInputShadowNode;

	void setTextShaper(std::shared_ptr<const RNTextShaper> shaper);

protected:
	Size measureContent(const LayoutContext &layoutContext, const LayoutConstraints &layoutConstraints) const override;

private:
	std::shared_ptr<const RNTextShaper> shaper_;
};

} // namespace facebook::react
