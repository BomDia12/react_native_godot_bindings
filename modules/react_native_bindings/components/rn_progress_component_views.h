#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_texture_sources.h"

#include "scene/gui/progress_bar.h"
#include "scene/gui/texture_progress_bar.h"

class RNProgressBarControl : public ProgressBar {
	GDCLASS(RNProgressBarControl, ProgressBar);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};

class RNTextureProgressBarControl : public TextureProgressBar {
	GDCLASS(RNTextureProgressBarControl, TextureProgressBar);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};

class RNProgressBarComponentView : public RNComponentView {
	RNProgressBarControl *bar;

public:
	explicit RNProgressBarComponentView(facebook::react::Tag p_tag);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
};

class RNTextureProgressBarComponentView : public RNComponentView {
	RNTextureProgressBarControl *bar;
	RNTextureSources sources;

	void _set_texture(int p_slot, const Ref<Texture2D> &p_texture);

public:
	RNTextureProgressBarComponentView(facebook::react::Tag p_tag, std::shared_ptr<RNImageService> p_images);
	~RNTextureProgressBarComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
};
