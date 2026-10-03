#pragma once

#include "rn_native_module_registry.h"

#include <atomic>
#include <map>

class RNBlobService : public std::enable_shared_from_this<RNBlobService> {
	struct Entry {
		PackedByteArray bytes;
		int64_t written = 0;
		bool complete = false;
		bool owner = true;
		uint32_t pins = 0;
		std::shared_ptr<std::atomic<bool>> collected = std::make_shared<std::atomic<bool>>(false);
	};
	std::map<String, Entry> blobs;
	std::map<String, Dictionary> urls;
	uint64_t maximum;
	uint64_t used = 0;
	uint64_t next_id = 1;
	bool closed = false;
	void reclaim(const String &p_id);

public:
	explicit RNBlobService(uint64_t p_maximum) : maximum(p_maximum) {}
	bool begin(const String &p_id, int64_t p_size, RNError &r_error);
	bool append(const String &p_id, int64_t p_offset, const PackedByteArray &p_bytes, RNError &r_error);
	bool finish(const String &p_id, RNError &r_error);
	Dictionary store(const PackedByteArray &p_bytes, RNError &r_error);
	bool read(const Dictionary &p_data, int64_t p_offset, int64_t p_length, PackedByteArray &r_bytes, RNError &r_error) const;
	bool pin(const Dictionary &p_data, RNError &r_error);
	void unpin(const String &p_id);
	void release(const String &p_id);
	std::weak_ptr<std::atomic<bool>> collector_flag(const String &p_id) const;
	void drain_releases();
	bool has_pending_work() const;
	String create_url(const Dictionary &p_data, RNError &r_error);
	void revoke_url(const String &p_url);
	bool resolve_url(const String &p_url, Dictionary &r_data, RNError &r_error) const;
	void shutdown();
	uint64_t used_bytes() const { return used; }
	uint64_t count() const { return blobs.size(); }
	std::shared_ptr<facebook::jsi::HostObject> collector_provider();
};
bool rn_register_blob_module(RNNativeModuleRegistry &p_registry, const std::function<std::shared_ptr<RNBlobService>()> &p_service, RNError &r_error);
