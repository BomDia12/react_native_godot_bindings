#pragma once

#include "rn_error.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"

#include <memory>

class Object;
struct RNRuntimeCoordinatorState;

struct RNSessionRecord {
	String token;
	uint64_t generation = 0;
	int root_tag = 0;
	uint64_t surface_epoch = 0;
	ObjectID root_view_id;
	bool open = true;
};

struct RNObjectRecord {
	String token;
	uint64_t generation = 0;
	String session_token;
	ObjectID object_id;
	String capability;
};

class RNObjectRegistry {
	std::weak_ptr<RNRuntimeCoordinatorState> state;
	uint64_t generation = 0;
	uint64_t next_token = 1;
	HashMap<String, RNSessionRecord> sessions;
	HashMap<String, RNObjectRecord> objects;

	String issue_token(const char *p_prefix);

public:
	explicit RNObjectRegistry(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state);

	void begin_generation(uint64_t p_generation);
	String open_session(int p_root_tag, RNError &r_error);
	bool close_session(const String &p_token);
	bool resolve_session(const String &p_token, RNSessionRecord &r_session, RNError &r_error) const;
	bool session_matches(const String &p_token, int p_root_tag, uint64_t p_epoch) const;
	String register_object(const String &p_session_token, ObjectID p_object, const String &p_capability, RNError &r_error);
	Object *resolve_object(const String &p_token, const String &p_session_token, const String &p_capability, RNError &r_error);
	void unregister_object(ObjectID p_object);
	void unregister_handle(const String &p_token) { objects.erase(p_token); }
	void close_surface(int p_root_tag, uint64_t p_epoch);
	void clear_generation();
	int session_count() const {
		int count = 0;
		for (const auto &entry : sessions) {
			count += entry.value.open ? 1 : 0;
		}
		return count;
	}
	int object_count() const { return objects.size(); }
	uint64_t get_generation() const { return generation; }
};
