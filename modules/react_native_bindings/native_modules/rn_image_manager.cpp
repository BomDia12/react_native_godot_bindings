#include "rn_image_manager.h"

#include "../host/rn_main_thread_queue.h"

#include <react/renderer/imagemanager/ImageResponseObserverCoordinator.h>

using namespace facebook::react;

bool rn_normalize_image_source(const ImageSource &p_source, RNImageSource &r_source, RNError &r_error) {
	Dictionary source;
	source["uri"] = String::utf8(p_source.uri.c_str());
	source["scale"] = p_source.scale;
	Dictionary headers;
	for (const auto &[name, value] : p_source.headers) {
		headers[String::utf8(name.c_str())] = String::utf8(value.c_str());
	}
	source["headers"] = headers;
	return RNImageService::normalize(source, r_source, r_error);
}

RNImageRequests::RNImageRequests(std::shared_ptr<RNImageService> p_service, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
		service(std::move(p_service)), main_thread(std::move(p_main_thread)) {
}

void RNImageRequests::_on_main_thread(std::function<void()> p_work) {
	if (main_thread->is_main_thread()) {
		p_work();
	} else {
		main_thread->post(std::move(p_work));
	}
}

void RNImageRequests::load(const ImageSource &p_source, const std::weak_ptr<Slot> &p_slot) {
	_on_main_thread([weak = weak_from_this(), p_source, p_slot] {
		const std::shared_ptr<RNImageRequests> self = weak.lock();
		const std::shared_ptr<Slot> slot = p_slot.lock();
		const std::shared_ptr<const ImageResponseObserverCoordinator> coordinator = slot ? slot->coordinator.lock() : nullptr;
		if (!self || !coordinator) {
			return;
		}
		RNImageSource source;
		RNError error;
		if (!rn_normalize_image_source(p_source, source, error)) {
			coordinator->nativeImageResponseFailed(ImageLoadError(std::make_shared<std::string>(error.message.utf8().get_data())));
			return;
		}
		slot->subscription = self->service->request(source, [p_slot](RNImageResult p_result) {
			const std::shared_ptr<Slot> loaded = p_slot.lock();
			const std::shared_ptr<const ImageResponseObserverCoordinator> observed = loaded ? loaded->coordinator.lock() : nullptr;
			if (!observed) {
				return;
			}
			loaded->subscription = 0;
			if (p_result.error.is_set()) {
				observed->nativeImageResponseFailed(ImageLoadError(std::make_shared<std::string>(p_result.error.message.utf8().get_data())));
			} else {
				observed->nativeImageResponseComplete(ImageResponse(std::const_pointer_cast<RNImageResource>(p_result.resource), nullptr));
			}
		});
	});
}

void RNImageRequests::cancel(uint64_t p_subscription) {
	if (!p_subscription) {
		return;
	}
	_on_main_thread([weak = weak_from_this(), p_subscription] {
		if (const std::shared_ptr<RNImageRequests> self = weak.lock()) {
			self->service->cancel(p_subscription);
		}
	});
}

RNImageRequests::Slot::~Slot() {
	if (const std::shared_ptr<RNImageRequests> alive = requests.lock()) {
		alive->cancel(subscription);
	}
}

RNImageManager::RNImageManager(std::shared_ptr<RNImageService> p_service, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
		ImageManager(nullptr), requests(std::make_shared<RNImageRequests>(std::move(p_service), std::move(p_main_thread))) {
}

ImageRequest RNImageManager::requestImage(const ImageSource &p_source, SurfaceId, const ImageRequestParams &, Tag) const {
	auto slot = std::make_shared<RNImageRequests::Slot>();
	slot->requests = requests;
	const std::weak_ptr<RNImageRequests> weak = requests;
	ImageRequest request(
			p_source, nullptr,
			SharedFunction<>([weak, p_source, slot] {
				if (auto state = weak.lock()) {
					state->load(p_source, slot);
				}
			}),
			SharedFunction<>([weak, slot] {
				if (auto state = weak.lock()) {
					state->cancel(slot->subscription.exchange(0));
				}
			}));
	if (p_source.type == ImageSource::Type::Invalid || p_source.uri.empty()) {
		return request;
	}
	slot->coordinator = request.getSharedObserverCoordinator();
	requests->load(p_source, slot);
	return request;
}
