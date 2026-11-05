#pragma once

#include "components/rn_rich_text_measurer.h"
#include "components/rn_theme_metrics.h"

#include <react/renderer/components/RNCSlider/EventEmitters.h>
#include <react/renderer/components/RNCSlider/Props.h>
#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/core/LayoutConstraints.h>

#include <cstring>

namespace facebook::react {

constexpr const char *GODOT_THEME_METRICS_KEY = "GodotThemeMetrics";
constexpr const char *RICH_TEXT_MEASURER_KEY = "GodotRichTextMeasurer";

// A leaf that takes its size from Godot's theme, as its mounted control would.
template <const char *ComponentName, typename PropsType, typename EmitterType, const char *(*ThemeClass)(const PropsType &)>
class GodotThemeSizedShadowNode final : public ConcreteViewShadowNode<ComponentName, PropsType, EmitterType> {
	using Base = ConcreteViewShadowNode<ComponentName, PropsType, EmitterType>;

public:
	GodotThemeSizedShadowNode(const ShadowNodeFragment &fragment, const ShadowNodeFamily::Shared &family, ShadowNodeTraits traits) :
			Base(fragment, family, traits) {}
	GodotThemeSizedShadowNode(const ShadowNode &source, const ShadowNodeFragment &fragment) :
			Base(source, fragment) {
		const auto &previous = static_cast<const PropsType &>(*source.getProps());
		const PropsType &next = this->getConcreteProps();
		if (previous.godotThemeType != next.godotThemeType || strcmp(ThemeClass(previous), ThemeClass(next)) != 0) {
			this->dirtyLayout();
		}
	}

	static ShadowNodeTraits BaseTraits() {
		auto traits = Base::BaseTraits();
		traits.set(ShadowNodeTraits::Trait::LeafYogaNode);
		traits.set(ShadowNodeTraits::Trait::MeasurableYogaNode);
		return traits;
	}

	void setThemeMetrics(std::shared_ptr<const RNThemeMetricsStore> p_theme) {
		this->ensureUnsealed();
		theme_ = std::move(p_theme);
	}

	Size measureContent(const LayoutContext &, const LayoutConstraints &layoutConstraints) const override {
		if (!theme_) {
			return layoutConstraints.clamp({ .width = 0, .height = 0 });
		}
		const PropsType &props = this->getConcreteProps();
		const Size2 size = theme_->get(this->getSurfaceId())->get_type(ThemeClass(props), props.godotThemeType).minimum_size;
		return layoutConstraints.clamp({ .width = size.x, .height = size.y });
	}

private:
	std::shared_ptr<const RNThemeMetricsStore> theme_;
};

template <typename ShadowNodeType>
class GodotThemeSizedComponentDescriptor final : public ConcreteComponentDescriptor<ShadowNodeType> {
	using Base = ConcreteComponentDescriptor<ShadowNodeType>;

public:
	explicit GodotThemeSizedComponentDescriptor(const ComponentDescriptorParameters &parameters) :
			Base(parameters),
			theme_(this->contextContainer_->template at<std::shared_ptr<const RNThemeMetricsStore>>(GODOT_THEME_METRICS_KEY)) {}

protected:
	void adopt(ShadowNode &shadowNode) const override {
		static_cast<ShadowNodeType &>(shadowNode).setThemeMetrics(theme_);
		Base::adopt(shadowNode);
	}

private:
	const std::shared_ptr<const RNThemeMetricsStore> theme_;
};

extern const char GodotProgressBarComponentName[];
inline const char *godot_progress_bar_theme_class(const GodotProgressBarProps &) {
	return "ProgressBar";
}
using GodotProgressBarShadowNode = GodotThemeSizedShadowNode<GodotProgressBarComponentName, GodotProgressBarProps, GodotProgressBarEventEmitter, godot_progress_bar_theme_class>;
using GodotProgressBarComponentDescriptor = GodotThemeSizedComponentDescriptor<GodotProgressBarShadowNode>;

// Applications use @react-native-community/slider, whose host component this is.
extern const char GodotSliderComponentName[];
inline const char *godot_slider_theme_class(const RNCSliderProps &p_props) {
	return p_props.vertical ? "VSlider" : "HSlider";
}
using GodotSliderShadowNode = GodotThemeSizedShadowNode<GodotSliderComponentName, RNCSliderProps, RNCSliderEventEmitter, godot_slider_theme_class>;
using GodotSliderComponentDescriptor = GodotThemeSizedComponentDescriptor<GodotSliderShadowNode>;

extern const char GodotRichTextComponentName[];

// Lays its BBCode out as the mounted RichTextLabel will, on the JS thread.
class GodotRichTextShadowNode final : public ConcreteViewShadowNode<GodotRichTextComponentName, GodotRichTextProps, GodotRichTextEventEmitter> {
public:
	GodotRichTextShadowNode(const ShadowNodeFragment &fragment, const ShadowNodeFamily::Shared &family, ShadowNodeTraits traits) :
			ConcreteViewShadowNode(fragment, family, traits) {}
	GodotRichTextShadowNode(const ShadowNode &source, const ShadowNodeFragment &fragment);

	static ShadowNodeTraits BaseTraits() {
		auto traits = ConcreteViewShadowNode::BaseTraits();
		traits.set(ShadowNodeTraits::Trait::LeafYogaNode);
		traits.set(ShadowNodeTraits::Trait::MeasurableYogaNode);
		return traits;
	}

	void setMeasuring(std::shared_ptr<const RNThemeMetricsStore> p_theme, std::shared_ptr<RNRichTextMeasurer> p_measurer) {
		ensureUnsealed();
		theme_ = std::move(p_theme);
		measurer_ = std::move(p_measurer);
	}

	Size measureContent(const LayoutContext &layoutContext, const LayoutConstraints &layoutConstraints) const override;

private:
	std::shared_ptr<const RNThemeMetricsStore> theme_;
	std::shared_ptr<RNRichTextMeasurer> measurer_;
};

class GodotRichTextComponentDescriptor final : public ConcreteComponentDescriptor<GodotRichTextShadowNode> {
public:
	explicit GodotRichTextComponentDescriptor(const ComponentDescriptorParameters &parameters) :
			ConcreteComponentDescriptor<GodotRichTextShadowNode>(parameters),
			theme_(contextContainer_->at<std::shared_ptr<const RNThemeMetricsStore>>(GODOT_THEME_METRICS_KEY)),
			measurer_(contextContainer_->at<std::shared_ptr<RNRichTextMeasurer>>(RICH_TEXT_MEASURER_KEY)) {}

protected:
	void adopt(ShadowNode &shadowNode) const override {
		static_cast<GodotRichTextShadowNode &>(shadowNode).setMeasuring(theme_, measurer_);
		ConcreteComponentDescriptor::adopt(shadowNode);
	}

private:
	const std::shared_ptr<const RNThemeMetricsStore> theme_;
	const std::shared_ptr<RNRichTextMeasurer> measurer_;
};

} // namespace facebook::react
