#pragma once

#include "rn_image_service.h"

#include <react/renderer/imagemanager/ImageManager.h>

#include <atomic>
#include <memory>

class RNMainThreadQueue;

// The loads behind upstream image requests. RNImageService belongs to the main
// thread, so every load and cancellation runs there.
class RNImageRequests : public std::enable_shared_from_this<RNImageRequests> {
public:
	// One request's load. Only the request's coordinator owns it, so a request
	// that upstream discards, as it does when it requests a source twice,
	// cancels its load.
	struct Slot {
		std::atomic<uint64_t> subscription = 0;
		std::weak_ptr<const facebook::react::ImageResponseObserverCoordinator> coordinator;
		std::weak_ptr<RNImageRequests> requests;
		~Slot();
	};

private:
	// Weak, so the last owner of the service is the host, on the main thread.
	std::weak_ptr<RNImageService> service;
	std::shared_ptr<RNMainThreadQueue> main_thread;

	void _on_main_thread(std::function<void()> p_work);

public:
	RNImageRequests(std::shared_ptr<RNImageService> p_service, std::shared_ptr<RNMainThreadQueue> p_main_thread);
	void load(const facebook::react::ImageSource &p_source, const std::weak_ptr<Slot> &p_slot);
	void cancel(uint64_t p_subscription);
};

// Serves the requests of upstream's ImageShadowNode, which picks the source
// that fits the laid-out size and stores the request in its ImageState.
class RNImageManager : public facebook::react::ImageManager {
	std::shared_ptr<RNImageRequests> requests;

public:
	RNImageManager(std::shared_ptr<RNImageService> p_service, std::shared_ptr<RNMainThreadQueue> p_main_thread);
	facebook::react::ImageRequest requestImage(const facebook::react::ImageSource &p_source, facebook::react::SurfaceId p_surface, const facebook::react::ImageRequestParams &p_params, facebook::react::Tag p_tag) const override;
};
