#pragma once

#include "rn_error.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"

class Object;

struct RNObjectRecord {
	int root_tag = 0;
	ObjectID object_id;
	String capability;
};

// Opaque handles that let one React surface refer to Godot objects with a
// declared capability. Handles never resolve from another surface.
class RNObjectRegistry {
	uint64_t next_token = 1;
	HashMap<String, RNObjectRecord> objects;

public:
	String register_object(int p_root_tag, ObjectID p_object, const String &p_capability, RNError &r_error);
	Object *resolve_object(const String &p_token, int p_root_tag, const String &p_capability, RNError &r_error);
	void unregister_handle(const String &p_token) { objects.erase(p_token); }
	void unregister_object(ObjectID p_object);
	void close_surface(int p_root_tag);
	int object_count() const { return objects.size(); }
};
