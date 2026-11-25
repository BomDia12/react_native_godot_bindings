#pragma once

#include "../mounting/rn_component_view.h"
#include "rn_image_control.h"

#include <react/renderer/imagemanager/ImageResponseObserverCoordinator.h>
#include <react/renderer/imagemanager/primitives.h>

class RNMainThreadQueue;

// Shows the request that upstream's ImageShadowNode keeps in its ImageState,
// observing it on the main thread until the request or the view goes away.
class RNImageComponentView : public RNComponentView {
	class Observer;
	RNImageControl *image;
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::shared_ptr<const facebook::react::ImageResponseObserverCoordinator> coordinator;
	std::shared_ptr<Observer> observer;
	facebook::react::ImageSource source;
	void _apply_corners();
	void _observe(const std::shared_ptr<const facebook::react::ImageResponseObserverCoordinator> &p_coordinator);
	void _loaded(const std::shared_ptr<const RNImageResource> &p_resource, const String &p_error);
	float _asset_scale() const;

public:
	RNImageComponentView(facebook::react::Tag p_tag, std::shared_ptr<RNMainThreadQueue> p_main_thread);
	~RNImageComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_state(const facebook::react::State::Shared &p_state) override;
	void update_layout(const facebook::react::LayoutMetrics &p_metrics) override;
};
