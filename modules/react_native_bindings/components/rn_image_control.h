#pragma once
#include "../native_modules/rn_image_service.h"
#include "rn_rounded_rect.h"

#include "scene/gui/texture_rect.h"
#include "scene/resources/material.h"

#include <memory>
class RNImageControl : public TextureRect {
	GDCLASS(RNImageControl, TextureRect);
	std::shared_ptr<const RNImageResource> resource;
	RNRoundedRect corners;
	Ref<ShaderMaterial> rounding;
	void _update_rounding();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNImageControl();
	// Shows p_resource, keeping its texture and memory reservation alive.
	void set_resource(std::shared_ptr<const RNImageResource> p_resource);
	// Rounds the image to p_corners, in local coordinates.
	void set_corners(const RNRoundedRect &p_corners);
};
