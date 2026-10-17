#pragma once

#include "../interop/rn_error.h"

#include "core/variant/variant.h"

#include <jsi/jsi.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

struct RNBlobCollectorState {
	std::atomic<uint32_t> references{ 0 };
	std::atomic<bool> collected{ false };
};

class RNBlobService : public std::enable_shared_from_this<RNBlobService> {
	struct Entry {
		PackedByteArray bytes;
		bool owner = true;
		uint32_t pins = 0;
		std::shared_ptr<RNBlobCollectorState> collectors = std::make_shared<RNBlobCollectorState>();
	};
	std::map<String, Entry> blobs;
	uint64_t maximum;
	uint64_t used = 0;
	uint64_t next_id = 1;
	bool closed = false;
	mutable std::recursive_mutex mutex;
	void reclaim(const String &p_id);

public:
	explicit RNBlobService(uint64_t p_maximum) :
			maximum(p_maximum) {}
	bool create(const String &p_id, const PackedByteArray &p_bytes, RNError &r_error);
	Dictionary store(const PackedByteArray &p_bytes, RNError &r_error);
	bool read(const Dictionary &p_data, int64_t p_offset, int64_t p_length, PackedByteArray &r_bytes, RNError &r_error) const;
	bool pin(const Dictionary &p_data, RNError &r_error);
	void unpin(const String &p_id);
	void release(const String &p_id);
	std::weak_ptr<RNBlobCollectorState> collector_state(const String &p_id) const;
	void drain_releases();
	bool has_pending_work() const;
	void shutdown();
	uint64_t used_bytes() const { return used; }
	uint64_t count() const { return blobs.size(); }
	std::shared_ptr<facebook::jsi::HostObject> collector_provider();
};
