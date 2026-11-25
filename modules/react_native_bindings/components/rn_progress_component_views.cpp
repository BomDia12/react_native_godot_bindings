#include "rn_progress_component_views.h"

#include <react/renderer/components/ReactNativeGodotSpec/Props.h>

using namespace facebook::react;

namespace {

enum TextureSlot {
	TEXTURE_UNDER,
	TEXTURE_PROGRESS,
	TEXTURE_OVER,
	TEXTURE_COUNT,
};

// Godot rounds the value to the step, so the range comes first.
void apply_range(Range *p_range, double p_minimum, double p_maximum, double p_step, double p_value) {
	p_range->set_min(p_minimum);
	p_range->set_max(p_maximum);
	p_range->set_step(p_step);
	p_range->set_value_no_signal(p_value);
	p_range->queue_redraw();
}

} // namespace

RNProgressBarComponentView::RNProgressBarComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNProgressBarControl)), bar(Object::cast_to<RNProgressBarControl>(get_control())) {
}

void RNProgressBarComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const GodotProgressBarProps &>(*p_new);
	apply_range(bar, next.minimumValue, next.maximumValue, next.step, next.value);
	bar->set_fill_mode(int(next.fillMode));
	bar->set_show_percentage(next.showPercentage);
	bar->set_theme_type_variation(String::utf8(next.godotThemeType.c_str()));
}

RNTextureProgressBarComponentView::RNTextureProgressBarComponentView(Tag p_tag, std::shared_ptr<RNImageService> p_images) :
		RNComponentView(p_tag, memnew(RNTextureProgressBarControl)),
		bar(Object::cast_to<RNTextureProgressBarControl>(get_control())),
		sources(std::move(p_images), TEXTURE_COUNT, [this](int p_slot, const Ref<Texture2D> &p_texture) { _set_texture(p_slot, p_texture); }, [this](int, const String &p_message) { ERR_PRINT(vformat("E_RESOURCE: GodotTextureProgressBar on React tag %d: %s", get_tag(), p_message)); }) {
}

RNTextureProgressBarComponentView::~RNTextureProgressBarComponentView() {
	sources.clear_callbacks();
}

void RNTextureProgressBarComponentView::_set_texture(int p_slot, const Ref<Texture2D> &p_texture) {
	switch (p_slot) {
		case TEXTURE_UNDER:
			bar->set_under_texture(p_texture);
			break;
		case TEXTURE_PROGRESS:
			bar->set_progress_texture(p_texture);
			break;
		default:
			bar->set_over_texture(p_texture);
			break;
	}
}

void RNTextureProgressBarComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const GodotTextureProgressBarProps &>(*p_new);
	apply_range(bar, next.minimumValue, next.maximumValue, next.step, next.value);
	bar->set_fill_mode(int(next.fillMode));
	sources.set_source(TEXTURE_UNDER, next.underTexture);
	sources.set_source(TEXTURE_PROGRESS, next.progressTexture);
	sources.set_source(TEXTURE_OVER, next.overTexture);
	const ::Color opaque(1, 1, 1);
	bar->set_tint_under(rn_godot_color(next.underTint, opaque));
	bar->set_tint_progress(rn_godot_color(next.progressTint, opaque));
	bar->set_tint_over(rn_godot_color(next.overTint, opaque));
	bar->set_radial_initial_angle(next.radialInitialAngle);
	bar->set_fill_degrees(next.radialFillDegrees);
	bar->set_radial_center_offset(Point2(next.radialCenterOffsetX, next.radialCenterOffsetY));
	bar->set_nine_patch_stretch(next.nineSliceStretch);
	bar->set_stretch_margin(SIDE_LEFT, next.stretchMarginLeft);
	bar->set_stretch_margin(SIDE_TOP, next.stretchMarginTop);
	bar->set_stretch_margin(SIDE_RIGHT, next.stretchMarginRight);
	bar->set_stretch_margin(SIDE_BOTTOM, next.stretchMarginBottom);
}
