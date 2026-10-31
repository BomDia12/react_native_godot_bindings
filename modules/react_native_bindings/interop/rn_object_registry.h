#pragma once

#include "rn_error.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"

class Object;

// Who a scene binding and its object handles belong to: one running root, or
// one application module that every root shares.
struct RNBindingScope {
	int root_tag = 0;
	String module;

	RNBindingScope(int p_root_tag = 0) :
			root_tag(p_root_tag) {}
	static RNBindingScope of_module(const String &p_module) {
		RNBindingScope scope;
		scope.module = p_module;
		return scope;
	}
	bool is_module() const { return !module.is_empty(); }
	bool operator==(const RNBindingScope &p_other) const { return root_tag == p_other.root_tag && module == p_other.module; }
	bool operator<(const RNBindingScope &p_other) const { return root_tag != p_other.root_tag ? root_tag < p_other.root_tag : module < p_other.module; }
};

struct RNObjectRecord {
	RNBindingScope scope;
	ObjectID object_id;
	String capability;
};

// Opaque handles that let one React surface or application module refer to
// Godot objects with a declared capability. Handles never resolve from
// another scope.
class RNObjectRegistry {
	uint64_t next_token = 1;
	HashMap<String, RNObjectRecord> objects;

public:
	String register_object(const RNBindingScope &p_scope, ObjectID p_object, const String &p_capability, RNError &r_error);
	Object *resolve_object(const String &p_token, const RNBindingScope &p_scope, const String &p_capability, RNError &r_error);
	void unregister_handle(const String &p_token) { objects.erase(p_token); }
	void unregister_object(ObjectID p_object);
	void close_scope(const RNBindingScope &p_scope);
	int object_count() const { return objects.size(); }
};
