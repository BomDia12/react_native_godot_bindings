#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_image_control.h"

class RNImageComponentView : public RNComponentView {
	RNImageControl *image;
	std::shared_ptr<RNImageService> service;

public:
	RNImageComponentView(facebook::react::Tag p_tag, std::shared_ptr<RNImageService> p_service);
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
};
