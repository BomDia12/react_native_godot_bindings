#pragma once

#include <react/io/IImageLoader.h>

#include <memory>

class RNImageService;
class RNMainThreadQueue;

namespace facebook::react {

class RNImageLoader : public IImageLoader {
	std::shared_ptr<RNImageService> service;
	std::shared_ptr<RNMainThreadQueue> main_thread;

public:
	RNImageLoader(std::shared_ptr<RNImageService> p_service, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
			service(std::move(p_service)), main_thread(std::move(p_main_thread)) {}

	void loadImage(const std::string &uri, const IImageLoaderOnLoadCallback &&onLoad) override;
	CacheStatus getCacheStatus(const std::string &uri) override;
};

} // namespace facebook::react
