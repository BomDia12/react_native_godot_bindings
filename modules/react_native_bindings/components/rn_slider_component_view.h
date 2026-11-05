#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_texture_sources.h"

#include "scene/gui/slider.h"

#include <react/renderer/components/RNCSlider/Props.h>

#include <functional>

template <typename SliderType>
class RNSliderControlBase : public SliderType {
public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};

class RNHSliderControl : public RNSliderControlBase<HSlider> {
	GDCLASS(RNHSliderControl, HSlider);

protected:
	static void _bind_methods() {}
};

class RNVSliderControl : public RNSliderControlBase<VSlider> {
	GDCLASS(RNVSliderControl, VSlider);

protected:
	static void _bind_methods() {}
};

// A host, because switching orientation replaces the Godot slider while React
// keeps the same view.
class RNSliderHost : public Control {
	GDCLASS(RNSliderHost, Control);

	std::function<void()> drag_started;
	std::function<void()> drag_ended;
	std::function<void(double)> value_changed;
	Slider *slider = nullptr;

	void _drag_started();
	void _drag_ended(bool p_changed);
	void _value_changed(double p_value);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	void set_vertical(bool p_vertical);
	Slider *get_slider() const { return slider; }
	void set_callbacks(std::function<void()> p_started, std::function<void()> p_ended, std::function<void(double)> p_changed);
};

class RNSliderComponentView : public RNComponentView {
	RNSliderHost *host;
	RNTextureSources sources;
	double lower_limit = 0;
	double upper_limit = 0;
	double range_sum = 0;
	bool inverted = false;
	Ref<Texture2D> thumb_texture;
	Ref<Texture2D> track_texture;
	Ref<Texture2D> minimum_track_texture;
	Ref<Texture2D> maximum_track_texture;
	ObjectID thumb_slider;
	Ref<Texture2D> thumb_source;
	float thumb_built_size = 0;
	Color thumb_built_tint;

	void _apply_thumb(Slider *p_slider, const facebook::react::RNCSliderProps &p_props);
	void _set_texture(int p_slot, const Ref<Texture2D> &p_texture);
	void _apply_appearance();
	double _react_value(double p_slider_value) const;
	double _slider_value(double p_react_value) const;
	double _clamped(double p_react_value) const;

public:
	RNSliderComponentView(facebook::react::Tag p_tag, std::shared_ptr<RNImageService> p_images);
	~RNSliderComponentView() override;
	bool owns_activation() const override { return true; }
	Vector<Control *> get_input_controls() const override { return { host->get_slider() }; }
	bool owns_input_control(Control *p_control) const override { return p_control == host || p_control == host->get_slider(); }
	Control *get_focus_control() const override { return host->get_slider(); }
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
};
