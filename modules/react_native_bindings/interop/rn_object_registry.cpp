#include "rn_object_registry.h"

#include "core/object/object.h"

String RNObjectRegistry::register_object(const RNBindingScope &p_scope, ObjectID p_object, const String &p_capability, RNError &r_error) {
	if (!ObjectDB::get_instance(p_object)) {
		r_error = RNError::make(RNErrorCode::OBJECT_GONE, "object no longer exists", "registerObject", "object");
		return String();
	}
	for (const KeyValue<String, RNObjectRecord> &entry : objects) {
		if (entry.value.scope == p_scope && entry.value.object_id == p_object && entry.value.capability == p_capability) {
			return entry.key;
		}
	}
	if (objects.size() >= 4096) {
		Vector<String> destroyed;
		for (const KeyValue<String, RNObjectRecord> &entry : objects) {
			if (!ObjectDB::get_instance(entry.value.object_id)) {
				destroyed.push_back(entry.key);
			}
		}
		for (const String &token : destroyed) {
			objects.erase(token);
		}
		if (objects.size() >= 4096) {
			r_error = RNError::make(RNErrorCode::LIMIT, "object handle limit exceeded", "registerObject");
			return String();
		}
	}
	const String token = vformat("object-%016x", next_token++);
	objects[token] = { p_scope, p_object, p_capability };
	return token;
}

Object *RNObjectRegistry::resolve_object(const String &p_token, const RNBindingScope &p_scope, const String &p_capability, RNError &r_error) {
	const RNObjectRecord *record = objects.getptr(p_token);
	if (!record || !(record->scope == p_scope)) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "object handle is stale or belongs to another root or module", "resolveObject", "target");
		return nullptr;
	}
	if (record->capability != p_capability) {
		r_error = RNError::make(RNErrorCode::VALIDATION, vformat("object does not provide capability '%s'", p_capability), "resolveObject", "target");
		return nullptr;
	}
	Object *object = ObjectDB::get_instance(record->object_id);
	if (!object) {
		objects.erase(p_token);
		r_error = RNError::make(RNErrorCode::OBJECT_GONE, "registered object was destroyed", "resolveObject", "target");
		return nullptr;
	}
	return object;
}

void RNObjectRegistry::unregister_object(ObjectID p_object) {
	Vector<String> revoked;
	for (const KeyValue<String, RNObjectRecord> &entry : objects) {
		if (entry.value.object_id == p_object) {
			revoked.push_back(entry.key);
		}
	}
	for (const String &token : revoked) {
		objects.erase(token);
	}
}

void RNObjectRegistry::close_scope(const RNBindingScope &p_scope) {
	Vector<String> revoked;
	for (const KeyValue<String, RNObjectRecord> &entry : objects) {
		if (entry.value.scope == p_scope) {
			revoked.push_back(entry.key);
		}
	}
	for (const String &token : revoked) {
		objects.erase(token);
	}
}
