#include "rn_blob_service.h"

#include "core/os/os.h"

#include <cstring>

namespace {
bool failure(RNError &r_error, const String &p_message, const String &p_code = RNErrorCode::VALIDATION) {
	r_error = RNError::make(p_code, p_message, "Blob");
	return false;
}
String blob_id(const Dictionary &p_data) {
	return p_data.get("blobId", String());
}
class BlobCollector : public facebook::jsi::HostObject {
	std::weak_ptr<RNBlobCollectorState> state;

public:
	explicit BlobCollector(std::weak_ptr<RNBlobCollectorState> p_state) :
			state(std::move(p_state)) {
		if (auto owner = state.lock()) {
			owner->references.fetch_add(1);
		}
	}
	~BlobCollector() override {
		if (auto owner = state.lock()) {
			if (owner->references.fetch_sub(1) == 1) {
				owner->collected.store(true);
			}
		}
	}
};
class BlobCollectorProvider : public facebook::jsi::HostObject {
	std::weak_ptr<RNBlobService> service;

public:
	explicit BlobCollectorProvider(const std::shared_ptr<RNBlobService> &p_service) :
			service(p_service) {}
	facebook::jsi::Value get(facebook::jsi::Runtime &p_runtime, const facebook::jsi::PropNameID &p_name) override {
		if (p_name.utf8(p_runtime) != "create") {
			return facebook::jsi::Value::undefined();
		}
		const auto weak = service;
		return facebook::jsi::Function::createFromHostFunction(p_runtime, facebook::jsi::PropNameID::forAscii(p_runtime, "create"), 1, [weak](auto &rt, const auto &, const auto *args, size_t count) -> facebook::jsi::Value {
			if (count != 1 || !args[0].isString()) {
				throw facebook::jsi::JSError(rt, "Blob collector requires an ID.");
			}
			const String id = String::utf8(args[0].getString(rt).utf8(rt).c_str());
			auto owner = weak.lock();
			if (!owner) {
				throw facebook::jsi::JSError(rt, "Blob service is closed.");
			}
			auto flag = owner->collector_state(id);
			if (flag.expired()) {
				throw facebook::jsi::JSError(rt, "Blob collector requires live backing storage.");
			}
			return facebook::jsi::Object::createFromHostObject(rt, std::make_shared<BlobCollector>(flag));
		});
	}
};
} //namespace
bool RNBlobService::begin(const String &p_id, int64_t p_size, RNError &r_error) {
	if (closed || p_id.is_empty() || blobs.count(p_id)) {
		return failure(r_error, "Blob ID is stale or already allocated", RNErrorCode::STALE_HANDLE);
	}
	if (p_id.length() > 256 || blobs.size() >= 4096 || p_size < 0 || uint64_t(p_size) > maximum || used > maximum - uint64_t(p_size)) {
		return failure(r_error, "Blob aggregate limit exceeded", RNErrorCode::LIMIT);
	}
	Entry entry;
	entry.bytes.resize(p_size);
	blobs.emplace(p_id, std::move(entry));
	used += p_size;
	return true;
}
bool RNBlobService::append(const String &p_id, int64_t p_offset, const PackedByteArray &p_bytes, RNError &r_error) {
	auto found = blobs.find(p_id);
	if (found == blobs.end() || found->second.complete) {
		return failure(r_error, "Blob construction is unavailable", RNErrorCode::STALE_HANDLE);
	}
	Entry &entry = found->second;
	if (p_offset != entry.written || p_bytes.size() > 1024 * 1024 || p_bytes.size() > entry.bytes.size() - entry.written) {
		return failure(r_error, "Invalid bounded Blob write");
	}
	if (!p_bytes.is_empty()) {
		std::memcpy(entry.bytes.ptrw() + p_offset, p_bytes.ptr(), p_bytes.size());
	}
	entry.written += p_bytes.size();
	return true;
}
bool RNBlobService::finish(const String &p_id, RNError &r_error) {
	auto found = blobs.find(p_id);
	if (found == blobs.end() || found->second.written != found->second.bytes.size()) {
		return failure(r_error, "Blob construction is incomplete");
	}
	found->second.complete = true;
	return true;
}
Dictionary RNBlobService::store(const PackedByteArray &p_bytes, RNError &r_error) {
	const String id = vformat("native-%016x", next_id++);
	if (!begin(id, p_bytes.size(), r_error)) {
		return Dictionary();
	}
	Entry &entry = blobs.at(id);
	entry.bytes = p_bytes;
	entry.written = p_bytes.size();
	entry.complete = true;
	Dictionary data;
	data["blobId"] = id;
	data["offset"] = 0;
	data["size"] = p_bytes.size();
	data["type"] = "";
	return data;
}
bool RNBlobService::read(const Dictionary &p_data, int64_t p_offset, int64_t p_length, PackedByteArray &r_bytes, RNError &r_error) const {
	const String id = blob_id(p_data);
	auto found = blobs.find(id);
	if (closed || found == blobs.end() || !found->second.complete || (!found->second.owner && !found->second.pins)) {
		return failure(r_error, "Blob backing storage has been released", RNErrorCode::STALE_HANDLE);
	}
	if (p_data.get("offset", Variant()).get_type() != Variant::INT || p_data.get("size", Variant()).get_type() != Variant::INT) {
		return failure(r_error, "Blob descriptor requires integer offsets");
	}
	const int64_t offset = p_data["offset"], size = p_data["size"];
	if (offset < 0 || size < 0 || offset > found->second.bytes.size() || size > found->second.bytes.size() - offset || p_offset < 0 || p_offset > size || p_length < 0 || p_length > size - p_offset || p_length > 1024 * 1024) {
		return failure(r_error, "Invalid bounded Blob read");
	}
	r_bytes.resize(p_length);
	if (p_length) {
		std::memcpy(r_bytes.ptrw(), found->second.bytes.ptr() + offset + p_offset, p_length);
	}
	return true;
}
bool RNBlobService::pin(const Dictionary &p_data, RNError &r_error) {
	PackedByteArray empty;
	if (!read(p_data, 0, 0, empty, r_error)) {
		return false;
	}
	++blobs.at(blob_id(p_data)).pins;
	return true;
}
void RNBlobService::reclaim(const String &p_id) {
	auto found = blobs.find(p_id);
	if (found != blobs.end() && !found->second.owner && found->second.pins == 0) {
		used -= found->second.bytes.size();
		blobs.erase(found);
	}
}
void RNBlobService::unpin(const String &p_id) {
	auto found = blobs.find(p_id);
	if (found != blobs.end() && found->second.pins) {
		--found->second.pins;
		reclaim(p_id);
	}
}
void RNBlobService::release(const String &p_id) {
	auto found = blobs.find(p_id);
	if (found != blobs.end()) {
		found->second.owner = false;
		reclaim(p_id);
	}
}
std::weak_ptr<RNBlobCollectorState> RNBlobService::collector_state(const String &p_id) const {
	auto found = blobs.find(p_id);
	return found != blobs.end() ? std::weak_ptr<RNBlobCollectorState>(found->second.collectors) : std::weak_ptr<RNBlobCollectorState>();
}
void RNBlobService::drain_releases() {
	std::vector<String> collected;
	for (const auto &entry : blobs) {
		if (entry.second.collectors->collected.exchange(false) && entry.second.collectors->references.load() == 0) {
			collected.push_back(entry.first);
		}
	}
	for (const String &id : collected) {
		release(id);
	}
}
bool RNBlobService::has_pending_work() const {
	for (const auto &entry : blobs) {
		if (entry.second.collectors->collected.load()) {
			return true;
		}
	}
	return false;
}
String RNBlobService::create_url(const Dictionary &p_data, RNError &r_error) {
	if (urls.size() >= 4096) {
		failure(r_error, "Object URL count limit exceeded", RNErrorCode::LIMIT);
		return String();
	}
	if (!pin(p_data, r_error)) {
		return String();
	}
	const String url = vformat("blob:godot-%016x", next_id++);
	urls[url] = p_data.duplicate(true);
	return url;
}
void RNBlobService::revoke_url(const String &p_url) {
	auto found = urls.find(p_url);
	if (found != urls.end()) {
		const String id = blob_id(found->second);
		urls.erase(found);
		unpin(id);
	}
}
bool RNBlobService::resolve_url(const String &p_url, Dictionary &r_data, RNError &r_error) const {
	auto found = urls.find(p_url);
	if (found == urls.end()) {
		return failure(r_error, "Object URL is revoked or stale", RNErrorCode::STALE_HANDLE);
	}
	r_data = found->second;
	return true;
}
void RNBlobService::shutdown() {
	closed = true;
	urls.clear();
	blobs.clear();
	used = 0;
	drain_releases();
}
std::shared_ptr<facebook::jsi::HostObject> RNBlobService::collector_provider() {
	return std::make_shared<BlobCollectorProvider>(shared_from_this());
}

namespace {
class RNBlobModule : public RNNativeModule {
	std::function<std::shared_ptr<RNBlobService>()> service;

public:
	explicit RNBlobModule(std::function<std::shared_ptr<RNBlobService>()> p_service) :
			service(std::move(p_service)) {}
	RNModuleResult invoke_sync(const StringName &p_method, const Array &p_args, const RNCallContext &) override {
		auto owner = service();
		if (!owner) {
			return RNModuleResult::failure(RNError::make(RNErrorCode::RUNTIME_RESET, "Blob service unavailable", "Blob"));
		}
		RNError error;
		Variant result;
		if (p_method == "begin") {
			owner->begin(p_args[0], p_args[1], error);
		} else if (p_method == "append") {
			owner->append(p_args[0], p_args[1], p_args[2], error);
		} else if (p_method == "finish") {
			owner->finish(p_args[0], error);
		} else if (p_method == "release") {
			owner->release(p_args[0]);
		} else if (p_method == "pin") {
			owner->pin(p_args[0], error);
		} else if (p_method == "unpin") {
			owner->unpin(p_args[0]);
		} else if (p_method == "readChunk") {
			PackedByteArray bytes;
			owner->read(p_args[0], p_args[1], p_args[2], bytes, error);
			result = bytes;
		} else if (p_method == "createURL") {
			result = owner->create_url(p_args[0], error);
		} else if (p_method == "revokeURL") {
			owner->revoke_url(p_args[0]);
		} else if (p_method == "stats") {
			owner->drain_releases();
			Dictionary stats;
			stats["bytes"] = int64_t(owner->used_bytes());
			stats["count"] = int64_t(owner->count());
			result = stats;
		}
		return error.is_set() ? RNModuleResult::failure(error) : RNModuleResult::success(result);
	}
	void process_frame(double) override {
		if (auto owner = service()) {
			owner->drain_releases();
		}
	}
	bool has_pending_work() const override {
		auto owner = service();
		return owner && owner->has_pending_work();
	}
};
} //namespace
bool rn_register_blob_module(RNNativeModuleRegistry &p_registry, const std::function<std::shared_ptr<RNBlobService>()> &p_service, RNError &r_error) {
	RNModuleDefinition definition;
	definition.name = "GodotBinary";
	definition.factory = [p_service] { return std::make_unique<RNBlobModule>(p_service); };
	for (const char *name : { "begin", "append", "finish", "release", "readChunk", "createURL", "revokeURL", "pin", "unpin", "stats" }) {
		RNMethodSchema method;
		method.name = name;
		method.result = RNValueSchema::value(String(name) == "readChunk" ? RNValueType::BYTES : String(name) == "createURL" ? RNValueType::STRING
						: String(name) == "stats"																			? RNValueType::DYNAMIC
																															: RNValueType::VOID);
		if (String(name) != "stats") {
			RNArgumentSchema arg;
			arg.name = "data";
			arg.value = RNValueSchema::value(String(name) == "readChunk" || String(name) == "createURL" || String(name) == "pin" ? RNValueType::DYNAMIC : RNValueType::STRING);
			method.arguments.push_back(arg);
		}
		if (String(name) == "begin" || String(name) == "append" || String(name) == "readChunk") {
			RNArgumentSchema arg;
			arg.name = "offset";
			arg.value = RNValueSchema::value(RNValueType::INTEGER);
			method.arguments.push_back(arg);
		}
		if (String(name) == "append" || String(name) == "readChunk") {
			RNArgumentSchema arg;
			arg.name = "chunk";
			arg.value = RNValueSchema::value(String(name) == "append" ? RNValueType::BYTES : RNValueType::INTEGER);
			method.arguments.push_back(arg);
		}
		definition.methods.push_back(method);
	}
	return p_registry.register_module(definition, r_error);
}
