#include "rn_resource_requests.h"

#include "../interop/rn_resource_path.h"

#include "core/io/resource_loader.h"
#include "core/templates/hash_map.h"

namespace {

struct RNResourceSubscriber {
	uint64_t id = 0;
	RNResourceRequests::Completion completion;
};

// Keyed by path. A load nobody waits for anymore stays until the loader's
// request can be collected.
HashMap<String, Vector<RNResourceSubscriber>> loads;
uint64_t next_request = 1;

} // namespace

uint64_t RNResourceRequests::request(const String &p_path, const String &p_type_hint, Completion p_completion) {
	String path;
	RNError error;
	if (!rn_normalize_local_resource_path(p_path, path, error, "resource prop")) {
		p_completion(Ref<Resource>(), error.message);
		return 0;
	}
	if (ResourceCache::has(path)) {
		p_completion(ResourceLoader::load(path, p_type_hint), String());
		return 0;
	}
	if (!loads.has(path)) {
		if (ResourceLoader::load_threaded_request(path, p_type_hint) != OK) {
			p_completion(Ref<Resource>(), vformat("Could not load '%s'.", path));
			return 0;
		}
		loads.insert(path, Vector<RNResourceSubscriber>());
	}
	const uint64_t id = next_request++;
	loads[path].push_back({ id, std::move(p_completion) });
	return id;
}

void RNResourceRequests::cancel(uint64_t p_request) {
	if (p_request == 0) {
		return;
	}
	for (KeyValue<String, Vector<RNResourceSubscriber>> &load : loads) {
		for (int i = 0; i < load.value.size(); ++i) {
			if (load.value[i].id == p_request) {
				load.value.remove_at(i);
				return;
			}
		}
	}
}

void RNResourceRequests::poll() {
	Vector<String> finished;
	for (const KeyValue<String, Vector<RNResourceSubscriber>> &load : loads) {
		if (ResourceLoader::load_threaded_get_status(load.key) != ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
			finished.push_back(load.key);
		}
	}
	for (const String &path : finished) {
		const Vector<RNResourceSubscriber> subscribers = loads[path];
		loads.erase(path);
		const Ref<Resource> resource = ResourceLoader::load_threaded_get(path);
		const String error = resource.is_null() ? vformat("Could not load '%s'.", path) : String();
		for (const RNResourceSubscriber &subscriber : subscribers) {
			subscriber.completion(resource, error);
		}
	}
}

void RNResourceRequests::shutdown() {
	for (const KeyValue<String, Vector<RNResourceSubscriber>> &load : loads) {
		ResourceLoader::load_threaded_get(load.key);
	}
	loads.clear();
}
