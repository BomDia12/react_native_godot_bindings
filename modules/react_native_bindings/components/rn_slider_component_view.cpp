#include "rn_slider_component_view.h"

#include "core/object/callable_mp.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_texture.h"

#include <react/renderer/components/RNCSlider/EventEmitters.h>
#include <react/renderer/components/RNCSlider/Props.h>

using namespace facebook::react;

namespace {

enum SliderTexture {
	TEXTURE_THUMB,
	TEXTURE_TRACK,
	TEXTURE_MINIMUM_TRACK,
	TEXTURE_MAXIMUM_TRACK,
	TEXTURE_COUNT,
};

Ref<StyleBox> track_style(const Ref<Texture2D> &p_texture, const SharedColor &p_tint) {
	if (p_texture.is_valid()) {
		Ref<StyleBoxTexture> style;
		style.instantiate();
		style->set_texture(p_texture);
		return style;
	}
	if (p_tint) {
		Ref<StyleBoxFlat> style;
		style.instantiate();
		style->set_bg_color(rn_godot_color(p_tint));
		style->set_content_margin_all(2);
		return style;
	}
	return Ref<StyleBox>();
}

void override_style(Control *p_control, const StringName &p_name, const Ref<StyleBox> &p_style) {
	if (p_style.is_valid()) {
		p_control->add_theme_style_override(p_name, p_style);
	} else {
		p_control->remove_theme_style_override(p_name);
	}
}

void override_icon(Control *p_control, const StringName &p_name, const Ref<Texture2D> &p_icon) {
	if (p_icon.is_valid()) {
		p_control->add_theme_icon_override(p_name, p_icon);
	} else {
		p_control->remove_theme_icon_override(p_name);
	}
}

} // namespace

void RNSliderHost::set_callbacks(std::function<void()> p_started, std::function<void()> p_ended, std::function<void(double)> p_changed) {
	drag_started = std::move(p_started);
	drag_ended = std::move(p_ended);
	value_changed = std::move(p_changed);
}

void RNSliderHost::_drag_started() {
	if (drag_started) {
		drag_started();
	}
}

void RNSliderHost::_drag_ended(bool) {
	if (drag_ended) {
		drag_ended();
	}
}

void RNSliderHost::_value_changed(double p_value) {
	if (value_changed) {
		value_changed(p_value);
	}
}

void RNSliderHost::set_vertical(bool p_vertical) {
	const bool current = Object::cast_to<VSlider>(slider) != nullptr;
	if (slider && current == p_vertical) {
		return;
	}
	if (slider) {
		remove_child(slider);
		memdelete(slider);
	}
	slider = p_vertical ? static_cast<Slider *>(memnew(RNVSliderControl)) : static_cast<Slider *>(memnew(RNHSliderControl));
	add_child(slider, false, INTERNAL_MODE_FRONT);
	slider->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	slider->set_mouse_filter(MOUSE_FILTER_PASS);
	slider->connect("drag_started", callable_mp(this, &RNSliderHost::_drag_started));
	slider->connect("drag_ended", callable_mp(this, &RNSliderHost::_drag_ended));
	slider->connect(SceneStringName(value_changed), callable_mp(this, &RNSliderHost::_value_changed));
}

RNSliderComponentView::RNSliderComponentView(Tag p_tag, std::shared_ptr<RNImageService> p_images) :
		RNComponentView(p_tag, memnew(RNSliderHost)),
		host(Object::cast_to<RNSliderHost>(get_control())),
		sources(std::move(p_images), TEXTURE_COUNT, [this](int p_slot, const Ref<Texture2D> &p_texture) { _set_texture(p_slot, p_texture); }, [this](int, const String &p_message) { ERR_PRINT(vformat("E_RESOURCE: Slider on React tag %d: %s", get_tag(), p_message)); }) {
	host->set_vertical(false);
}

RNSliderComponentView::~RNSliderComponentView() {
	sources.clear_callbacks();
	if (has_control()) {
		host->set_callbacks(nullptr, nullptr, nullptr);
	}
}

void RNSliderComponentView::_set_texture(int p_slot, const Ref<Texture2D> &p_texture) {
	switch (p_slot) {
		case TEXTURE_THUMB:
			thumb_texture = p_texture;
			break;
		case TEXTURE_TRACK:
			track_texture = p_texture;
			break;
		case TEXTURE_MINIMUM_TRACK:
			minimum_track_texture = p_texture;
			break;
		default:
			maximum_track_texture = p_texture;
			break;
	}
	_apply_appearance();
}

void RNSliderComponentView::_apply_appearance() {
	if (!props) {
		return;
	}
	const auto &next = static_cast<const RNCSliderProps &>(*props);
	Slider *slider = host->get_slider();
	_apply_thumb(slider, next);
	const Ref<StyleBox> minimum_track = track_style(minimum_track_texture, next.minimumTrackTintColor);
	const Ref<Texture2D> background = maximum_track_texture.is_valid() ? maximum_track_texture : track_texture;
	override_style(slider, SNAME("slider"), track_style(background, next.maximumTrackTintColor));
	override_style(slider, SNAME("grabber_area"), minimum_track);
	override_style(slider, SNAME("grabber_area_highlight"), minimum_track);
}

// Resizing and tinting read the image back and upload a new texture, so a dragged
// slider, which gets new props on every tick, only does it when an input changed.
void RNSliderComponentView::_apply_thumb(Slider *p_slider, const RNCSliderProps &p_props) {
	const ::Color no_tint(1, 1, 1);
	const ::Color tint = rn_godot_color(p_props.thumbTintColor, no_tint);
	if (p_slider->get_instance_id() == thumb_slider && thumb_texture == thumb_source && p_props.thumbSize == thumb_built_size && tint == thumb_built_tint) {
		return;
	}
	thumb_slider = p_slider->get_instance_id();
	thumb_source = thumb_texture;
	thumb_built_size = p_props.thumbSize;
	thumb_built_tint = tint;
	const StringName icons[] = { SNAME("grabber"), SNAME("grabber_highlight"), SNAME("grabber_disabled") };
	for (const StringName &icon : icons) {
		p_slider->remove_theme_icon_override(icon);
	}
	const bool resizes = p_props.thumbSize > 0;
	for (const StringName &icon : icons) {
		Ref<Texture2D> base = thumb_texture.is_valid() ? thumb_texture : p_slider->get_theme_icon(icon);
		Ref<Image> image = (resizes || tint != no_tint) && base.is_valid() ? base->get_image() : Ref<Image>();
		if (image.is_null()) {
			override_icon(p_slider, icon, thumb_texture);
			continue;
		}
		image = image->duplicate();
		if (image->is_compressed()) {
			image->decompress();
		}
		image->convert(Image::FORMAT_RGBA8);
		if (resizes) {
			image->resize(int(p_props.thumbSize), int(p_props.thumbSize));
		}
		for (int y = 0; y < image->get_height(); ++y) {
			for (int x = 0; x < image->get_width(); ++x) {
				image->set_pixel(x, y, image->get_pixel(x, y) * tint);
			}
		}
		override_icon(p_slider, icon, ImageTexture::create_from_image(image));
	}
}

// Godot sliders can't run backwards, so an inverted slider shows the mirrored value.
double RNSliderComponentView::_react_value(double p_slider_value) const {
	return inverted ? range_sum - p_slider_value : p_slider_value;
}

double RNSliderComponentView::_slider_value(double p_react_value) const {
	return inverted ? range_sum - p_react_value : p_react_value;
}

double RNSliderComponentView::_clamped(double p_react_value) const {
	return CLAMP(p_react_value, lower_limit, upper_limit);
}

void RNSliderComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto old = std::static_pointer_cast<const RNCSliderProps>(p_old);
	const auto &next = static_cast<const RNCSliderProps &>(*p_new);
	host->set_vertical(next.vertical);
	Slider *slider = host->get_slider();
	inverted = next.inverted;
	range_sum = next.minimumValue + next.maximumValue;
	lower_limit = MAX(next.minimumValue, double(next.lowerLimit));
	upper_limit = next.upperLimit > next.lowerLimit ? MIN(next.maximumValue, double(next.upperLimit)) : next.maximumValue;
	slider->set_min(next.minimumValue);
	slider->set_max(next.maximumValue);
	slider->set_step(next.step);
	slider->set_editable(!next.disabled);
	slider->set_theme_type_variation(String::utf8(next.godotThemeType.c_str()));
	slider->set_value_no_signal(_slider_value(_clamped(next.value)));
	slider->queue_redraw();
	sources.set_source(TEXTURE_THUMB, next.thumbImage);
	sources.set_source(TEXTURE_TRACK, next.trackImage);
	sources.set_source(TEXTURE_MINIMUM_TRACK, next.minimumTrackImage);
	sources.set_source(TEXTURE_MAXIMUM_TRACK, next.maximumTrackImage);
	_apply_appearance();
}

void RNSliderComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const RNCSliderEventEmitter>(p_emitter);
	if (!emitter) {
		host->set_callbacks(nullptr, nullptr, nullptr);
		return;
	}
	host->set_callbacks(
			[this, emitter]() { emitter->onRNCSliderSlidingStart({ .value = Float(_react_value(host->get_slider()->get_value())), .fromUser = true }); },
			[this, emitter]() { emitter->onRNCSliderSlidingComplete({ .value = Float(_react_value(host->get_slider()->get_value())), .fromUser = true }); },
			[this, emitter](double p_value) {
				const double clamped = _clamped(_react_value(p_value));
				if (clamped != _react_value(p_value)) {
					host->get_slider()->set_value_no_signal(_slider_value(clamped));
				}
				const Float value = Float(clamped);
				// The package's JS sends onChange to the same handler, so one event is enough.
				emitter->onRNCSliderValueChange({ .value = value, .fromUser = true });
			});
}
