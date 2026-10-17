#pragma once

#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

namespace facebook::react {

extern const char GodotWindowComponentName[];

// The native window's current size; zero until the window reports one.
struct GodotWindowState {
	Size windowSize{};
};

class GodotWindowShadowNode final : public ConcreteViewShadowNode<GodotWindowComponentName, GodotWindowProps, GodotWindowEventEmitter, GodotWindowState> {
public:
	using ConcreteViewShadowNode::ConcreteViewShadowNode;

	static ShadowNodeTraits BaseTraits() {
		auto traits = ConcreteViewShadowNode::BaseTraits();
		traits.set(ShadowNodeTraits::Trait::RootNodeKind);
		return traits;
	}
};

// Children lay out inside the separate native window, so the node takes the
// window's size and leaves its parent's flow, as Modal does with the screen.
class GodotWindowComponentDescriptor final : public ConcreteComponentDescriptor<GodotWindowShadowNode> {
public:
	using ConcreteComponentDescriptor::ConcreteComponentDescriptor;

	void adopt(ShadowNode &shadowNode) const override {
		auto &layoutable = static_cast<YogaLayoutableShadowNode &>(shadowNode);
		const auto &window = static_cast<const GodotWindowShadowNode &>(shadowNode);
		const Size stateSize = window.getStateData().windowSize;
		const auto &props = window.getConcreteProps();
		const Size size = stateSize.width > 0 && stateSize.height > 0 ? stateSize : Size{ .width = Float(props.windowWidth), .height = Float(props.windowHeight) };
		layoutable.setSize(size);
		layoutable.setPositionType(YGPositionTypeAbsolute);
		ConcreteComponentDescriptor::adopt(shadowNode);
	}
};

} // namespace facebook::react
