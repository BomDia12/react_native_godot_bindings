#include "rn_image_service.h"

#include "rn_native_module_registry.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/os/thread.h"
#include "scene/resources/compressed_texture.h"
#include "scene/resources/image_texture.h"

namespace {
constexpr uint64_t WORKSPACE_BYTES = 32 * 1024 * 1024;
RNImageResult failure(const String &p_code, const String &p_message) {
	RNImageResult result;
	result.error = RNError::make(p_code, p_message, "image.load");
	return result;
}
Image::LoadLimits codec_limits(const RNImageLimits &p_limits) {
	Image::LoadLimits limits;
	limits.max_encoded_bytes = p_limits.encoded_bytes;
	limits.max_allocation_bytes = p_limits.decoded_bytes;
	limits.max_workspace_bytes = WORKSPACE_BYTES;
	return limits;
}
} //namespace
bool RNImageLimits::valid() const {
	return encoded_bytes > 0 && encoded_bytes <= 16 * 1024 * 1024 && decoded_bytes > 0 && decoded_bytes <= INT32_MAX && total_bytes > 0 && total_bytes <= INT32_MAX && cache_bytes <= total_bytes && concurrent_decodes > 0 && concurrent_decodes <= 32;
}
bool RNImageBudget::reserve(uint64_t p_bytes) {
	if (p_bytes > maximum - MIN(used, maximum)) {
		return false;
	}
	used += p_bytes;
	return true;
}
void RNImageBudget::release(uint64_t p_bytes) {
	used -= MIN(used, p_bytes);
}
RNImageReservation::~RNImageReservation() {
	if (budget) {
		budget->release(bytes);
	}
}
bool RNImageReservation::resize(uint64_t p_bytes) {
	if (p_bytes > bytes && !budget->reserve(p_bytes - bytes)) {
		return false;
	}
	if (p_bytes < bytes) {
		budget->release(bytes - p_bytes);
	}
	bytes = p_bytes;
	return true;
}
RNImageService::RNImageService(const RNImageLimits &p_limits) : limits(p_limits), budget(std::make_shared<RNImageBudget>()) {
	budget->maximum = limits.valid() ? limits.total_bytes : 0;
}
RNImageService::~RNImageService() {
	if (transport) {
		for (const auto &entry : pending) {
			transport->cancel(entry.second.transport_id);
		}
	}
}
std::shared_ptr<RNImageService> RNImageService::for_generation(uint64_t p_generation) {
	static std::map<uint64_t, std::weak_ptr<RNImageService>> services;
	for (auto it = services.begin(); it != services.end();) {
		if (it->second.expired()) {
			it = services.erase(it);
		} else {
			++it;
		}
	}
	auto service = services[p_generation].lock();
	if (service) {
		return service;
	}
	RNImageLimits config;
	config.cache_bytes = GLOBAL_GET("react_native/images/cache_bytes");
	config.encoded_bytes = GLOBAL_GET("react_native/images/max_encoded_bytes");
	config.decoded_bytes = GLOBAL_GET("react_native/images/max_decoded_bytes");
	config.total_bytes = GLOBAL_GET("react_native/images/total_bytes");
	config.concurrent_decodes = GLOBAL_GET("react_native/images/max_concurrent_decodes");
	service = std::make_shared<RNImageService>(config);
	services[p_generation] = service;
	return service;
}
bool RNImageService::normalize(const Dictionary &p_source, RNImageSource &r_source, RNError &r_error) {
	r_source = RNImageSource();
	if (p_source.get("uri", Variant()).get_type() != Variant::STRING) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Image source requires a URI", "image.source");
		return false;
	}
	r_source.uri = p_source["uri"];
	r_source.scale = p_source.get("scale", 1.0);
	if (r_source.uri.is_empty() || r_source.uri.find_char(0) >= 0 || r_source.uri.contains("\\") || !Math::is_finite(r_source.scale) || r_source.scale <= 0 || r_source.scale > 64) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid image URI or scale", "image.source");
		return false;
	}
	r_source.remote = r_source.uri.begins_with("http://") || r_source.uri.begins_with("https://");
	if (!r_source.remote && !r_source.uri.begins_with("res://") && !r_source.uri.begins_with("user://") && !r_source.uri.begins_with("data:image/")) {
		r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Unsupported image URI scheme", "image.source");
		return false;
	}
	if (r_source.uri.begins_with("res://") || r_source.uri.begins_with("user://")) {
		const String path = r_source.uri.substr(r_source.uri.find("://") + 3);
		if (path.is_empty() || path.split("/").has("..")) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid local image path", "image.source");
			return false;
		}
		r_source.uri = r_source.uri.simplify_path();
	}
	if (p_source.has("headers") && p_source["headers"].get_type() != Variant::DICTIONARY) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Image headers require a record", "image.source");
		return false;
	}
	r_source.headers = Dictionary(p_source.get("headers", Dictionary())).duplicate(true);
	const String credentials = p_source.get("credentials", "include");
	if (credentials != "omit" && credentials != "include") {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Image credentials must be include or omit", "image.source");
		return false;
	}
	r_source.stateless = credentials == "omit";
	if (r_source.headers.size() > 128) {
		r_error = RNError::make(RNErrorCode::LIMIT, "Too many image headers", "image.source");
		return false;
	}
	Array keys = r_source.headers.keys();
	keys.sort();
	CryptoCore::SHA256Context identity;
	identity.start();
	auto hash_part = [&identity](const String &part) {
		const uint64_t length = part.length();
		identity.update(reinterpret_cast<const uint8_t *>(&length), sizeof(length));
		identity.update(reinterpret_cast<const uint8_t *>(part.ptr()), size_t(length) * sizeof(char32_t));
	};
	hash_part(r_source.uri);
	hash_part(String::num(r_source.scale, 9));
	for (const Variant &key : keys) {
		if (key.get_type() != Variant::STRING || r_source.headers[key].get_type() != Variant::STRING) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "Image headers must contain strings", "image.source");
			return false;
		}
		String name = key;
		String value = r_source.headers[key];
		if (name.length() > 256 || value.length() > 8192) {
			r_error = RNError::make(RNErrorCode::LIMIT, "Image header exceeds limit", "image.source");
			return false;
		}
		if (name.is_empty() || name.contains("\n") || name.contains("\r") || value.contains("\n") || value.contains("\r")) {
			r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid image header", "image.source");
			return false;
		}
		if (name.to_lower() == "authorization" || name.to_lower() == "cookie") {
			r_source.stateless = false;
		}
		hash_part(name.to_lower());
		hash_part(value);
	}
	for (const char *key : { "method", "body" }) {
		if (p_source.has(key)) {
			r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Image method/body options are unavailable", "image.source", key);
			return false;
		}
	}
	r_source.shared = !r_source.remote || r_source.stateless;
	if (!r_source.remote && !r_source.uri.begins_with("data:")) {
		const String imported = ResourceLoader::import_remap(r_source.uri);
		hash_part(String::num_uint64(FileAccess::get_modified_time(imported)));
		hash_part(String::num_uint64(FileAccess::get_size(imported)));
	}
	unsigned char digest[32];
	identity.finish(digest);
	r_source.key = String::hex_encode_buffer(digest, 32);
	return true;
}
bool RNImageService::evict(uint64_t p_required) {
	while (!cache.empty() && (p_required > budget->maximum - MIN(budget->used, budget->maximum) || cached_bytes > limits.cache_bytes)) {
		auto oldest = cache.begin();
		for (auto it = cache.begin(); it != cache.end(); ++it) {
			if (it->second.accessed < oldest->second.accessed) {
				oldest = it;
			}
		}
		cached_bytes -= oldest->second.resource->reservation->bytes;
		cache.erase(oldest);
	}
	return p_required <= budget->maximum - MIN(budget->used, budget->maximum);
}
std::shared_ptr<RNImageReservation> RNImageService::reserve(uint64_t p_bytes) {
	if (!evict(p_bytes)) {
		return {};
	}
	auto reservation = std::make_shared<RNImageReservation>();
	reservation->budget = budget;
	if (!reservation->resize(p_bytes)) {
		return {};
	}
	return reservation;
}
std::shared_ptr<const RNImageResource> RNImageService::cached(const RNImageSource &p_source) {
	if (!p_source.shared) {
		return {};
	}
	auto found = cache.find(p_source.key);
	if (found == cache.end()) {
		auto active = live.find(p_source.key);
		return active == live.end() ? std::shared_ptr<const RNImageResource>() : active->second.lock();
	}
	found->second.accessed = ++sequence;
	return found->second.resource;
}
void RNImageService::remember(const RNImageSource &p_source, const RNImageResult &p_result, bool p_cacheable) {
	for (auto it = live.begin(); it != live.end();) {
		if (it->second.expired()) {
			it = live.erase(it);
		} else {
			++it;
		}
	}
	if (p_source.shared && p_cacheable && p_result.resource) {
		live[p_source.key] = p_result.resource;
	}
	if (!p_source.shared || !p_cacheable || !p_result.resource || !limits.cache_bytes || p_result.resource->reservation->bytes > limits.cache_bytes) {
		return;
	}
	auto previous = cache.find(p_source.key);
	if (previous != cache.end()) {
		cached_bytes -= previous->second.resource->reservation->bytes;
	}
	cache[p_source.key] = { p_result.resource, ++sequence };
	cached_bytes += p_result.resource->reservation->bytes;
	evict(0);
}
RNImageResult RNImageService::decode(const RNImageSource &p_source, const Vector<uint8_t> &p_bytes, const std::shared_ptr<RNImageReservation> &p_reservation) {
	if (active_decodes >= limits.concurrent_decodes || p_bytes.size() <= 0 || uint64_t(p_bytes.size()) > limits.encoded_bytes) {
		return failure(RNErrorCode::LIMIT, "Image encoded/concurrent decode limit exceeded");
	}
	Image::ScopedLoadLimits codec_scope(codec_limits(limits));
	Size2i dimensions;
	Image::BufferFormat format;
	if (Image::probe_buffer(p_bytes.ptr(), p_bytes.size(), dimensions, format) != OK) {
		return failure(RNErrorCode::LIMIT, "Unsupported, corrupt or oversized image header");
	}
	const uint64_t pixels = uint64_t(dimensions.x) * uint64_t(dimensions.y);
	const uint64_t retained = pixels * 32 + 1024;
	const uint64_t required = WORKSPACE_BYTES + uint64_t(p_bytes.size()) * 3 + pixels * 64 + 1024;
	if (!evict(required > p_reservation->bytes ? required - p_reservation->bytes : 0) || !p_reservation->resize(required)) {
		return failure(RNErrorCode::LIMIT, "Live images leave insufficient decode/upload memory");
	}
	++active_decodes;
	Ref<Image> image;
	image.instantiate();
	Error error = format == Image::BUFFER_PNG ? image->load_png_from_buffer(p_bytes) : format == Image::BUFFER_JPEG ? image->load_jpg_from_buffer(p_bytes)
																													: image->load_webp_from_buffer(p_bytes);
	--active_decodes;
	if (error != OK || image->get_size() != dimensions) {
		return failure(RNErrorCode::NATIVE, "Native image decoding failed");
	}
	auto resource = std::make_shared<RNImageResource>();
	resource->pixels = dimensions;
	resource->texture = ImageTexture::create_from_image(image);
	if (resource->texture.is_null()) {
		return failure(RNErrorCode::NATIVE, "Texture upload failed");
	}
	resource->reservation = std::make_shared<RNImageReservation>();
	resource->reservation->budget = budget;
	resource->reservation->bytes = retained;
	p_reservation->bytes -= retained;
	image.unref();
	p_reservation->resize(uint64_t(p_bytes.size()) * 3);
	RNImageResult result;
	result.resource = resource;
	return result;
}
RNImageResult RNImageService::load_local(const RNImageSource &p_source) {
	if (!limits.valid()) {
		return failure(RNErrorCode::VALIDATION, "Invalid react_native/images ProjectSettings");
	}
	Vector<uint8_t> bytes;
	std::shared_ptr<RNImageReservation> reservation;
	if (p_source.uri.begins_with("data:")) {
		const int comma = p_source.uri.find(",");
		if (comma < 0 || !p_source.uri.substr(0, comma).ends_with(";base64")) {
			return failure(RNErrorCode::UNSUPPORTED, "Only base64 image data URLs are supported");
		}
		const uint64_t characters = p_source.uri.length() - comma - 1;
		const uint64_t bound = ((characters + 3) / 4) * 3;
		if (bound > limits.encoded_bytes) {
			return failure(RNErrorCode::LIMIT, "Image data URL exceeds encoded limit");
		}
		reservation = reserve(WORKSPACE_BYTES + bound * 3 + characters * 4);
		if (!reservation) {
			return failure(RNErrorCode::LIMIT, "Image data URL exceeds aggregate memory");
		}
		const CharString base64 = p_source.uri.substr(comma + 1).ascii();
		bytes.resize(bound);
		size_t decoded = 0;
		if (CryptoCore::b64_decode(bytes.ptrw(), bytes.size(), &decoded, reinterpret_cast<const uint8_t *>(base64.get_data()), base64.length()) != OK) {
			return failure(RNErrorCode::VALIDATION, "Invalid image base64");
		}
		bytes.resize(decoded);
	} else {
		String path = ResourceLoader::import_remap(p_source.uri);
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null()) {
			return failure(RNErrorCode::NATIVE, "Image file cannot be opened");
		}
		const uint64_t size = file->get_length();
		if (!size || size > limits.encoded_bytes) {
			return failure(RNErrorCode::LIMIT, "Image file exceeds encoded limit");
		}
		reservation = reserve(WORKSPACE_BYTES + size * 3);
		if (!reservation) {
			return failure(RNErrorCode::LIMIT, "Insufficient image input/workspace memory");
		}
		if (path.get_extension().to_lower() == "ctex") {
			if (size < 52 || file->get_32() != 0x32545347 || file->get_32() > CompressedTexture2D::FORMAT_VERSION) {
				return failure(RNErrorCode::VALIDATION, "Invalid imported texture header");
			}
			const uint32_t logical_width = file->get_32();
			const uint32_t logical_height = file->get_32();
			file->seek(36);
			const uint32_t storage = file->get_32();
			const uint32_t width = file->get_16();
			const uint32_t height = file->get_16();
			const uint32_t mips = file->get_32();
			const uint32_t format = file->get_32();
			Image::ScopedLoadLimits codec_scope(codec_limits(limits));
			if (!Image::is_load_size_allowed(width, height, 16, true) || !Image::is_load_size_allowed(logical_width, logical_height, 16, true) || storage > CompressedTexture2D::DATA_FORMAT_WEBP || format >= Image::FORMAT_MAX || mips > 32) {
				return failure(RNErrorCode::LIMIT, "Imported texture exceeds bounds or uses an unbounded codec");
			}
			const uint64_t pixels = uint64_t(width) * height;
			const uint64_t retained = pixels * 64 + 1024;
			const uint64_t required = WORKSPACE_BYTES + size * 3 + pixels * 128 + 1024;
			if (!evict(required - reservation->bytes) || !reservation->resize(required)) {
				return failure(RNErrorCode::LIMIT, "Imported texture exceeds aggregate memory");
			}
			Ref<Texture2D> texture = ResourceLoader::load(p_source.uri, "Texture2D", ResourceLoader::CACHE_MODE_IGNORE);
			if (texture.is_null()) {
				return failure(RNErrorCode::NATIVE, "Native imported texture load failed");
			}
			auto resource = std::make_shared<RNImageResource>();
			resource->texture = texture;
			resource->pixels = Size2i(width, height);
			resource->reservation = reservation;
			reservation->resize(retained);
			RNImageResult result;
			result.resource = resource;
			return result;
		}
		bytes.resize(size);
		if (file->get_buffer(bytes.ptrw(), size) != size) {
			return failure(RNErrorCode::NATIVE, "Truncated image file");
		}
	}
	return decode(p_source, bytes, reservation);
}
uint64_t RNImageService::request(const RNImageSource &p_source, std::function<void(RNImageResult)> p_completion) {
	if (auto resource = cached(p_source)) {
		RNImageResult result;
		result.resource = resource;
		p_completion(result);
		return 0;
	}
	if (!p_source.remote) {
		auto result = load_local(p_source);
		remember(p_source, result);
		p_completion(result);
		return 0;
	}
	if (!transport) {
		p_completion(failure(RNErrorCode::UNSUPPORTED, "Image HTTP transport is not installed"));
		return 0;
	}
	const uint64_t subscription = ++sequence;
	if (p_source.shared) {
		for (auto &entry : pending) {
			if (entry.second.source.shared && entry.second.source.key == p_source.key) {
				entry.second.subscribers[subscription] = std::move(p_completion);
				return subscription;
			}
		}
	}
	auto reservation = reserve(limits.encoded_bytes * 3 + WORKSPACE_BYTES);
	if (!reservation) {
		p_completion(failure(RNErrorCode::LIMIT, "Image request exceeds aggregate memory"));
		return 0;
	}
	Pending work;
	work.source = p_source;
	work.reservation = reservation;
	work.subscribers[subscription] = std::move(p_completion);
	pending[subscription] = std::move(work);
	std::weak_ptr<RNImageService> weak = shared_from_this();
	const auto held_reservation = reservation;
	uint64_t transport_id = transport->start(p_source, limits.encoded_bytes, [weak, subscription, held_reservation](RNImageTransportResponse response) { if (auto service = weak.lock()) { service->finish(subscription, std::move(response)); } });
	auto found = pending.find(subscription);
	if (found != pending.end()) {
		found->second.transport_id = transport_id;
	}
	return subscription;
}
void RNImageService::finish(uint64_t p_id, RNImageTransportResponse p_response) {
	auto found = pending.find(p_id);
	if (found == pending.end()) {
		return;
	}
	Pending work = std::move(found->second);
	pending.erase(found);
	if (work.subscribers.empty()) {
		return;
	}
	RNImageResult result = p_response.error.is_set() ? RNImageResult{ {}, p_response.error } : decode(work.source, p_response.bytes, work.reservation);
	remember(work.source, result, p_response.cacheable);
	for (const auto &subscriber : work.subscribers) {
		subscriber.second(result);
	}
}
void RNImageService::cancel(uint64_t p_subscription) {
	for (auto it = pending.begin(); it != pending.end(); ++it) {
		if (it->second.subscribers.erase(p_subscription)) {
			if (it->second.subscribers.empty() && transport->cancel(it->second.transport_id)) {
				pending.erase(it);
			}
			return;
		}
	}
}
void RNImageService::clear_cache() {
	cache.clear();
	live.clear();
	cached_bytes = 0;
}
void rn_register_image_settings() {
	for (const auto &entry : { std::pair<const char *, int64_t>("cache_bytes", 64 * 1024 * 1024), { "max_encoded_bytes", 8 * 1024 * 1024 }, { "max_decoded_bytes", 64 * 1024 * 1024 }, { "total_bytes", 256 * 1024 * 1024 }, { "max_concurrent_decodes", 2 } }) {
		GLOBAL_DEF(PropertyInfo(Variant::INT, String("react_native/images/") + entry.first, PROPERTY_HINT_RANGE, String(entry.first) == "max_concurrent_decodes" ? "1,32,1" : "0,2147483647,1"), entry.second);
	}
}

namespace {
class RNImageModule : public RNNativeModule {
	std::shared_ptr<RNImageService> service;
	std::shared_ptr<std::map<String, uint64_t>> subscriptions = std::make_shared<std::map<String, uint64_t>>();

public:
	~RNImageModule() {
		if (service) {
			const auto pending = *subscriptions;
			subscriptions->clear();
			for (const auto &entry : pending) {
				service->cancel(entry.second);
			}
		}
	}
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context) override {
		service = RNImageService::for_generation(p_context.generation);
		if (p_method == "queryCache") {
			Dictionary result;
			if (p_arguments.size() != 1 || p_arguments[0].get_type() != Variant::ARRAY) {
				return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "queryCache requires source records", "image.module"));
			}
			const Array sources = p_arguments[0];
			for (const Variant &value : sources) {
				if (value.get_type() != Variant::DICTIONARY) {
					return RNModuleResult::failure(RNError::make(RNErrorCode::VALIDATION, "queryCache requires source records", "image.module"));
				}
				const Dictionary source = value;
				RNImageSource normalized;
				RNError error;
				if (!RNImageService::normalize(source, normalized, error)) {
					return RNModuleResult::failure(error);
				}
				if (service->cached(normalized)) {
					result[normalized.uri] = "memory";
				}
			}
			return RNModuleResult::success(result);
		}
		return RNModuleResult::failure(RNError::make(RNErrorCode::UNSUPPORTED, "Unknown ImageLoader method", "image.module"));
	}
	void start_async(const StringName &p_method, const Array &p_arguments, const RNCallContext &p_context, const RNCompletionToken &p_completion) override {
		service = RNImageService::for_generation(p_context.generation);
		RNImageSource source;
		RNError error;
		if (p_arguments.size() != 1 || p_arguments[0].get_type() != Variant::DICTIONARY) {
			p_completion.fail(RNError::make(RNErrorCode::VALIDATION, "ImageLoader requires a source record", "image.module"));
			return;
		}
		if (!RNImageService::normalize(p_arguments[0], source, error)) {
			p_completion.fail(error);
			return;
		}
		auto completed = std::make_shared<bool>(false);
		const String token = p_context.request_token;
		uint64_t subscription = service->request(source, [p_completion, source, p_method, completed, token, tracking = subscriptions](RNImageResult result) { *completed = true; tracking->erase(token); if (result.error.is_set()) { p_completion.fail(result.error); } else if (p_method == "prefetch") { p_completion.complete(true); } else { Dictionary size; size["width"] = result.resource->texture->get_width() / source.scale; size["height"] = result.resource->texture->get_height() / source.scale; p_completion.complete(size); } });
		if (subscription && !*completed) {
			(*subscriptions)[p_context.request_token] = subscription;
		}
	}
	void cancel(const String &p_request) override {
		auto found = subscriptions->find(p_request);
		if (found != subscriptions->end()) {
			const uint64_t subscription = found->second;
			subscriptions->erase(found);
			service->cancel(subscription);
		}
	}
};
} //namespace
bool rn_register_image_module(RNNativeModuleRegistry &p_registry, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "GodotImageLoader";
	for (const char *name : { "getSize", "prefetch", "queryCache" }) {
		RNMethodSchema method;
		method.name = name;
		method.mode = String(name) == "queryCache" ? RNCallMode::SYNC : RNCallMode::ASYNC;
		RNArgumentSchema argument;
		argument.name = "source";
		argument.value = RNValueSchema::value(RNValueType::DYNAMIC);
		method.arguments.push_back(argument);
		method.result = RNValueSchema::value(RNValueType::DYNAMIC);
		definition.methods.push_back(method);
	}
	definition.factory = []() { return std::make_unique<RNImageModule>(); };
	return p_registry.register_module(definition, r_error);
}
