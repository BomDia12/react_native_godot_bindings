#pragma once

#include "components/rn_theme_metrics.h"

#include <react/renderer/components/FBReactNativeSpec/EventEmitters.h>
#include <react/renderer/components/FBReactNativeSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/core/LayoutConstraints.h>

namespace facebook::react {

extern const char GodotSwitchComponentName[];

// Switch's spec is interface-only, so codegen emits no shadow node; Godot sizes Switch from
// the CheckButton minimum size in the current theme, as the native view does.
class GodotSwitchShadowNode final : public ConcreteViewShadowNode<GodotSwitchComponentName, SwitchProps, SwitchEventEmitter> {
public:
	using ConcreteViewShadowNode::ConcreteViewShadowNode;

	static ShadowNodeTraits BaseTraits() {
		auto traits = ConcreteViewShadowNode::BaseTraits();
		traits.set(ShadowNodeTraits::Trait::LeafYogaNode);
		traits.set(ShadowNodeTraits::Trait::MeasurableYogaNode);
		return traits;
	}

	void setThemeMetrics(std::shared_ptr<const RNThemeMetricsStore> theme) {
		ensureUnsealed();
		theme_ = std::move(theme);
	}

	Size measureContent(const LayoutContext &, const LayoutConstraints &layoutConstraints) const override {
		const Size2 size = theme_ ? theme_->get(getSurfaceId())->switch_size : Size2();
		return layoutConstraints.clamp({ .width = size.x, .height = size.y });
	}

private:
	std::shared_ptr<const RNThemeMetricsStore> theme_;
};

class GodotSwitchComponentDescriptor final : public ConcreteComponentDescriptor<GodotSwitchShadowNode> {
public:
	static constexpr const char *THEME_KEY = "GodotThemeMetrics";

	explicit GodotSwitchComponentDescriptor(const ComponentDescriptorParameters &parameters) :
			ConcreteComponentDescriptor<GodotSwitchShadowNode>(parameters),
			theme_(contextContainer_->at<std::shared_ptr<const RNThemeMetricsStore>>(THEME_KEY)) {}

protected:
	void adopt(ShadowNode &shadowNode) const override {
		static_cast<GodotSwitchShadowNode &>(shadowNode).setThemeMetrics(theme_);
		ConcreteComponentDescriptor::adopt(shadowNode);
	}

private:
	const std::shared_ptr<const RNThemeMetricsStore> theme_;
};

} // namespace facebook::react
