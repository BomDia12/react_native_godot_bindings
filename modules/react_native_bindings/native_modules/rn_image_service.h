#pragma once
#include "rn_native_module.h"

class RNNativeModuleRegistry;

#include "core/io/image.h"
#include "scene/resources/texture.h"

#include <map>

struct RNImageSource {
	String uri;
	Dictionary headers;
	double scale = 1;
	bool remote = false;
	bool stateless = false;
	bool shared = true;
	String key;
};
struct RNImageLimits {
	uint64_t cache_bytes = 64 * 1024 * 1024;
	uint64_t encoded_bytes = 8 * 1024 * 1024;
	uint64_t decoded_bytes = 64 * 1024 * 1024;
	uint64_t total_bytes = 256 * 1024 * 1024;
	int concurrent_decodes = 2;
	bool valid() const;
};
struct RNImageBudget {
	uint64_t used = 0;
	uint64_t maximum = 0;
	bool reserve(uint64_t p_bytes);
	void release(uint64_t p_bytes);
};
struct RNImageReservation {
	std::shared_ptr<RNImageBudget> budget;
	uint64_t bytes = 0;
	~RNImageReservation();
	bool resize(uint64_t p_bytes);
};
struct RNImageResource {
	Ref<Texture2D> texture;
	Size2i pixels;
	std::shared_ptr<RNImageReservation> reservation;
};
struct RNImageResult {
	std::shared_ptr<const RNImageResource> resource;
	RNError error;
};
struct RNImageTransportResponse {
	Vector<uint8_t> bytes;
	RNError error;
	bool cacheable = true;
};
class RNImageTransport {
public:
	virtual ~RNImageTransport() = default;
	virtual uint64_t start(const RNImageSource &p_source, uint64_t p_encoded_limit, std::function<void(RNImageTransportResponse)> p_completion) = 0;
	virtual bool cancel(uint64_t p_request) = 0;
};
class RNImageService : public std::enable_shared_from_this<RNImageService> {
	struct CacheEntry {
		std::shared_ptr<const RNImageResource> resource;
		uint64_t accessed = 0;
	};
	struct Pending {
		RNImageSource source;
		uint64_t transport_id = 0;
		std::map<uint64_t, std::function<void(RNImageResult)>> subscribers;
		std::shared_ptr<RNImageReservation> reservation;
	};
	RNImageLimits limits;
	std::shared_ptr<RNImageBudget> budget;
	std::shared_ptr<RNImageTransport> transport;
	std::map<String, CacheEntry> cache;
	std::map<String, std::weak_ptr<const RNImageResource>> live;
	std::map<uint64_t, Pending> pending;
	uint64_t sequence = 0;
	int active_decodes = 0;
	uint64_t cached_bytes = 0;
	bool evict(uint64_t p_required);
	std::shared_ptr<RNImageReservation> reserve(uint64_t p_bytes);
	RNImageResult decode(const RNImageSource &p_source, const Vector<uint8_t> &p_bytes, const std::shared_ptr<RNImageReservation> &p_reservation);
	RNImageResult load_local(const RNImageSource &p_source);
	void finish(uint64_t p_id, RNImageTransportResponse p_response);
	void remember(const RNImageSource &p_source, const RNImageResult &p_result, bool p_cacheable = true);

public:
	explicit RNImageService(const RNImageLimits &p_limits);
	~RNImageService();
	static std::shared_ptr<RNImageService> for_generation(uint64_t p_generation);
	static bool normalize(const Dictionary &p_source, RNImageSource &r_source, RNError &r_error);
	void set_transport(std::shared_ptr<RNImageTransport> p_transport) { transport = std::move(p_transport); }
	uint64_t request(const RNImageSource &p_source, std::function<void(RNImageResult)> p_completion);
	void cancel(uint64_t p_subscription);
	std::shared_ptr<const RNImageResource> cached(const RNImageSource &p_source);
	uint64_t used_bytes() const { return budget->used; }
	void clear_cache();
};
bool rn_register_image_module(RNNativeModuleRegistry &p_registry, RNError &r_error);
void rn_register_image_settings();
