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
bool RNBlobService::create(const String &p_id, const PackedByteArray &p_bytes, RNError &r_error) {
	std::lock_guard lock(mutex);
	if (closed || p_id.is_empty() || blobs.count(p_id)) {
		return failure(r_error, "Blob ID is stale or already allocated", RNErrorCode::STALE_HANDLE);
	}
	const uint64_t size = p_bytes.size();
	if (p_id.length() > 256 || blobs.size() >= 4096 || size > maximum || used > maximum - size) {
		return failure(r_error, "Blob aggregate limit exceeded", RNErrorCode::LIMIT);
	}
	Entry entry;
	entry.bytes = p_bytes;
	blobs.emplace(p_id, std::move(entry));
	used += size;
	return true;
}
Dictionary RNBlobService::store(const PackedByteArray &p_bytes, RNError &r_error) {
	std::lock_guard lock(mutex);
	const String id = vformat("native-%016x", next_id++);
	if (!create(id, p_bytes, r_error)) {
		return Dictionary();
	}
	Dictionary data;
	data["blobId"] = id;
	data["offset"] = 0;
	data["size"] = p_bytes.size();
	data["type"] = "";
	return data;
}
bool RNBlobService::read(const Dictionary &p_data, int64_t p_offset, int64_t p_length, PackedByteArray &r_bytes, RNError &r_error) const {
	std::lock_guard lock(mutex);
	const String id = blob_id(p_data);
	auto found = blobs.find(id);
	if (closed || found == blobs.end() || (!found->second.owner && !found->second.pins)) {
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
	std::lock_guard lock(mutex);
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
	std::lock_guard lock(mutex);
	auto found = blobs.find(p_id);
	if (found != blobs.end() && found->second.pins) {
		--found->second.pins;
		reclaim(p_id);
	}
}
void RNBlobService::release(const String &p_id) {
	std::lock_guard lock(mutex);
	auto found = blobs.find(p_id);
	if (found != blobs.end()) {
		found->second.owner = false;
		reclaim(p_id);
	}
}
std::weak_ptr<RNBlobCollectorState> RNBlobService::collector_state(const String &p_id) const {
	std::lock_guard lock(mutex);
	auto found = blobs.find(p_id);
	return found != blobs.end() ? std::weak_ptr<RNBlobCollectorState>(found->second.collectors) : std::weak_ptr<RNBlobCollectorState>();
}
void RNBlobService::drain_releases() {
	std::lock_guard lock(mutex);
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
	std::lock_guard lock(mutex);
	for (const auto &entry : blobs) {
		if (entry.second.collectors->collected.load()) {
			return true;
		}
	}
	return false;
}
void RNBlobService::shutdown() {
	std::lock_guard lock(mutex);
	closed = true;
	blobs.clear();
	used = 0;
	drain_releases();
}
std::shared_ptr<facebook::jsi::HostObject> RNBlobService::collector_provider() {
	return std::make_shared<BlobCollectorProvider>(shared_from_this());
}
