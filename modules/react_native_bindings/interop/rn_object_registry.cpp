#include "rn_object_registry.h"

#include "../runtime/react_native_runtime_coordinator.h"

#include "core/object/object.h"

RNObjectRegistry::RNObjectRegistry(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) :
		state(p_state) {
}

String RNObjectRegistry::issue_token(const char *p_prefix) {
	return vformat("%s-%016x-%016x", p_prefix, generation, next_token++);
}

void RNObjectRegistry::begin_generation(uint64_t p_generation) {
	if (generation == p_generation) {
		return;
	}
	clear_generation();
	generation = p_generation;
}

String RNObjectRegistry::open_session(int p_root_tag, RNError &r_error) {
	auto shared = state.lock();
	if (!shared) {
		r_error = RNError::make(RNErrorCode::NATIVE, "runtime coordinator is unavailable", "openSession");
		return String();
	}
	auto route = shared->routes.find(p_root_tag);
	if (route == shared->routes.end() || route->second.runtime_generation != generation || (route->second.status != RNSurfaceStatus::ACTIVE && route->second.status != RNSurfaceStatus::STARTING)) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, vformat("root %d is not live in generation %d", p_root_tag, generation), "openSession", "rootTag");
		return String();
	}
	if (sessions.size() >= 4096) {
		Vector<String> closed;
		for (const auto &entry : sessions) {
			if (!entry.value.open) {
				closed.push_back(entry.key);
			}
		}
		for (const String &token : closed) {
			sessions.erase(token);
		}
		if (sessions.size() >= 4096) {
			r_error = RNError::make(RNErrorCode::LIMIT, "live session limit exceeded", "openSession");
			return String();
		}
	}
	RNSessionRecord record;
	record.token = issue_token("session");
	record.generation = generation;
	record.root_tag = p_root_tag;
	record.surface_epoch = route->second.surface_epoch;
	record.root_view_id = route->second.root_view_id;
	sessions[record.token] = record;
	return record.token;
}

bool RNObjectRegistry::close_session(const String &p_token) {
	RNSessionRecord *session = sessions.getptr(p_token);
	if (!session || !session->open) {
		return false;
	}
	session->open = false;
	Vector<String> revoked;
	for (const KeyValue<String, RNObjectRecord> &entry : objects) {
		if (entry.value.session_token == p_token) {
			revoked.push_back(entry.key);
		}
	}
	for (const String &token : revoked) {
		objects.erase(token);
	}
	return true;
}

bool RNObjectRegistry::resolve_session(const String &p_token, RNSessionRecord &r_session, RNError &r_error) const {
	const RNSessionRecord *session = sessions.getptr(p_token);
	if (!session || session->generation != generation) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "session token is stale or unknown", "resolveSession", "session");
		return false;
	}
	if (!session->open) {
		r_error = RNError::make(RNErrorCode::SESSION_CLOSED, "session is closed", "resolveSession", "session");
		return false;
	}
	auto shared = state.lock();
	if (!shared) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "session route is no longer live", "resolveSession", "session");
		return false;
	}
	auto route = shared->routes.find(session->root_tag);
	if (route == shared->routes.end() || route->second.runtime_generation != generation || route->second.surface_epoch != session->surface_epoch) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "session route is no longer live", "resolveSession", "session");
		return false;
	}
	r_session = *session;
	return true;
}

bool RNObjectRegistry::session_matches(const String &p_token, int p_root_tag, uint64_t p_epoch) const {
	const RNSessionRecord *session = sessions.getptr(p_token);
	return session && session->root_tag == p_root_tag && session->surface_epoch == p_epoch;
}

String RNObjectRegistry::register_object(const String &p_session_token, ObjectID p_object, const String &p_capability, RNError &r_error) {
	RNSessionRecord session;
	if (!resolve_session(p_session_token, session, r_error)) {
		return String();
	}
	if (!ObjectDB::get_instance(p_object)) {
		r_error = RNError::make(RNErrorCode::OBJECT_GONE, "object no longer exists", "registerObject", "object");
		return String();
	}
	for (const KeyValue<String, RNObjectRecord> &entry : objects) {
		if (entry.value.session_token == p_session_token && entry.value.object_id == p_object && entry.value.capability == p_capability) {
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
	RNObjectRecord record;
	record.token = issue_token("object");
	record.generation = generation;
	record.session_token = p_session_token;
	record.object_id = p_object;
	record.capability = p_capability;
	objects[record.token] = record;
	return record.token;
}

Object *RNObjectRegistry::resolve_object(const String &p_token, const String &p_session_token, const String &p_capability, RNError &r_error) {
	const RNObjectRecord *record = objects.getptr(p_token);
	if (!record || record->generation != generation || record->session_token != p_session_token) {
		r_error = RNError::make(RNErrorCode::STALE_HANDLE, "object token is stale or belongs to another session", "resolveObject", "target");
		return nullptr;
	}
	if (record->capability != p_capability) {
		r_error = RNError::make(RNErrorCode::VALIDATION, vformat("object does not provide capability '%s'", p_capability), "resolveObject", "target");
		return nullptr;
	}
	RNSessionRecord ignored;
	if (!resolve_session(p_session_token, ignored, r_error)) {
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

void RNObjectRegistry::close_surface(int p_root_tag, uint64_t p_epoch) {
	Vector<String> closing;
	for (const KeyValue<String, RNSessionRecord> &entry : sessions) {
		if (entry.value.root_tag == p_root_tag && entry.value.surface_epoch == p_epoch) {
			closing.push_back(entry.key);
		}
	}
	for (const String &token : closing) {
		close_session(token);
	}
}

void RNObjectRegistry::clear_generation() {
	objects.clear();
	sessions.clear();
	next_token = 1;
}
