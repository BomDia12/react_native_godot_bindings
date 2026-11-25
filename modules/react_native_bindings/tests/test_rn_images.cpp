#include "../components/rn_texture_sources.h"
#include "../host/rn_main_thread_queue.h"
#include "../native_modules/rn_image_manager.h"
#include "../turbomodules/rn_image_loader.h"
#include "support/rn_upstream_test_macros.h"

#include "core/crypto/crypto_core.h"

#include <react/renderer/imagemanager/ImageResponseObserverCoordinator.h>

namespace TestRNImages {

String data_uri() {
	Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0));
	const Vector<uint8_t> bytes = image->save_png_to_buffer();
	return "data:image/png;base64," + CryptoCore::b64_encode_str(bytes.ptr(), bytes.size());
}

class ImageObserver : public facebook::react::ImageResponseObserver {
public:
	mutable std::shared_ptr<const RNImageResource> received;
	void didReceiveProgress(float, int64_t, int64_t) const override {}
	void didReceiveImage(const facebook::react::ImageResponse &p_response) const override {
		received = std::static_pointer_cast<const RNImageResource>(p_response.getImage());
	}
	void didReceiveFailure(const facebook::react::ImageLoadError &) const override {}
};

facebook::react::ImageSource upstream_source(const String &p_uri, float p_scale) {
	facebook::react::ImageSource source;
	source.type = facebook::react::ImageSource::Type::Local;
	source.uri = p_uri.utf8().get_data();
	source.scale = p_scale;
	return source;
}

TEST_CASE("[ReactNativeBindings][Images] callers at different scales share one decoded image") {
	auto service = std::make_shared<RNImageService>(RNImageLimits());
	auto main_thread = std::make_shared<RNMainThreadQueue>();
	Ref<Texture2D> applied;
	RNTextureSources textures(service, 1, [&applied](int, const Ref<Texture2D> &p_texture) { applied = p_texture; }, nullptr);
	textures.set_source(0, upstream_source(data_uri(), 1));
	REQUIRE(applied.is_valid());
	const uint64_t used = service->used_bytes();

	RNImageManager manager(service, main_thread);
	const facebook::react::ImageRequest request = manager.requestImage(upstream_source(data_uri(), 2), 1, {}, 1);
	auto observer = std::make_shared<ImageObserver>();
	request.getObserverCoordinator().addObserver(observer);
	REQUIRE(observer->received);
	CHECK(observer->received->texture == applied);

	facebook::react::RNImageLoader loader(service, main_thread);
	Size2 size;
	loader.loadImage(data_uri().utf8().get_data(), [&size](double p_width, double p_height, const char *) { size = Size2(p_width, p_height); });
	main_thread->drain();
	CHECK(size == Size2(2, 2));
	CHECK(service->used_bytes() == used);
	request.getObserverCoordinator().removeObserver(observer);
}

TEST_CASE("[ReactNativeBindings][Images] a consumed image request resumes for a later observer") {
	auto service = std::make_shared<RNImageService>(RNImageLimits());
	RNImageManager manager(service, std::make_shared<RNMainThreadQueue>());
	const facebook::react::ImageRequest request = manager.requestImage(upstream_source(data_uri(), 1), 1, {}, 1);
	const facebook::react::ImageResponseObserverCoordinator &coordinator = request.getObserverCoordinator();
	auto first = std::make_shared<ImageObserver>();
	coordinator.addObserver(first);
	REQUIRE(first->received);
	coordinator.removeObserver(first);

	auto second = std::make_shared<ImageObserver>();
	coordinator.addObserver(second);
	CHECK(second->received == first->received);
	coordinator.removeObserver(second);
}
} // namespace TestRNImages

void rn_force_link_image_tests() {}
