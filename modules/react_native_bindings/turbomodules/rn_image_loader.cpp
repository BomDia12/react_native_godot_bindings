#include "rn_image_loader.h"

#include "../host/rn_main_thread_queue.h"
#include "../native_modules/rn_image_service.h"

namespace facebook::react {

namespace {

bool normalized_source(const std::string &p_uri, RNImageSource &r_source, RNError &r_error) {
	// Godot strings end at NUL, so a URI hiding one would be read as a shorter path.
	if (p_uri.find('\0') != std::string::npos) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid image URI or scale", "image.source");
		return false;
	}
	Dictionary source;
	source["uri"] = String::utf8(p_uri.data(), p_uri.size());
	return RNImageService::normalize(source, r_source, r_error);
}

} // namespace

void RNImageLoader::loadImage(const std::string &uri, const IImageLoaderOnLoadCallback &&onLoad) {
	main_thread->post([service = service, uri, onLoad = onLoad]() {
		RNImageSource source;
		RNError error;
		if (!normalized_source(uri, source, error)) {
			onLoad(0, 0, error.message.utf8().get_data());
			return;
		}
		service->request(source, [onLoad, scale = source.scale](RNImageResult p_result) {
			if (p_result.error.is_set()) {
				onLoad(0, 0, p_result.error.message.utf8().get_data());
				return;
			}
			const Size2i pixels = p_result.resource->pixels;
			onLoad(pixels.x / scale, pixels.y / scale, nullptr);
		});
	});
}

IImageLoader::CacheStatus RNImageLoader::getCacheStatus(const std::string &uri) {
	CacheStatus status = CacheStatus::None;
	main_thread->run_sync([&]() {
		RNImageSource source;
		RNError error;
		if (normalized_source(uri, source, error) && service->cached(source)) {
			status = CacheStatus::Memory;
		}
	});
	return status;
}

} // namespace facebook::react
