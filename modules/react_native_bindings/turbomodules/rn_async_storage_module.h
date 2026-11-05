#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include <AsyncStorageSpec/AsyncStorageSpecJSI.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// One database of @react-native-async-storage/async-storage: a JSON object
// under user://, loaded on first use and replaced whole on every write.
class RNStorageFile {
	String path;
	Dictionary values;
	bool loaded = false;

public:
	explicit RNStorageFile(const String &p_path) :
			path(p_path) {}
	// Both return an error message, which is empty on success.
	String load();
	String save();
	// Starts empty without reading the file, so a corrupt database can be cleared.
	String reset();
	Dictionary &get_values() { return values; }
};

// Runs storage work in order away from the JS and main threads. Destroying it
// finishes what was queued, so writes survive a reload.
class RNStorageThread {
	std::mutex mutex;
	std::condition_variable wake;
	std::deque<std::function<void()>> tasks;
	bool stopping = false;
	std::thread thread;

public:
	~RNStorageThread();
	void post(std::function<void()> p_task);
	// Queued work holds JS promises, which must not outlive their runtime.
	void wait_until_idle();
};

// The databases of the application. Runtimes share them, because an old runtime
// can still be writing while its reload starts the next one, and module lookups can
// make several instances. Only the storage thread reads the databases, and it stops
// before they go.
class RNAsyncStorageState {
	std::map<std::string, std::unique_ptr<RNStorageFile>> databases;

public:
	RNStorageThread thread;
	RNStorageFile &file(const std::optional<String> &p_database);
};

// Whether a database name can become a file name under the storage directory.
bool rn_storage_name_is_valid(const String &p_name);
// Merges p_source into p_target as AsyncStorage's merge does, recursively for objects.
Dictionary rn_storage_merge(const Dictionary &p_target, const Dictionary &p_source);

namespace facebook::react {

class RNAsyncStorageModule : public NativeAsyncStorageCxxSpec<RNAsyncStorageModule> {
	std::shared_ptr<RNAsyncStorageState> storage;

	template <typename Work>
	jsi::Value _run(jsi::Runtime &rt, const std::optional<String> &p_database, Work p_work, bool p_load = true);

public:
	RNAsyncStorageModule(std::shared_ptr<CallInvoker> jsInvoker, std::shared_ptr<RNAsyncStorageState> p_storage);
	jsi::Value getValues(jsi::Runtime &rt, jsi::String dbName, jsi::Array keys);
	jsi::Value setValues(jsi::Runtime &rt, jsi::String dbName, jsi::Array values);
	jsi::Value removeValues(jsi::Runtime &rt, jsi::String dbName, jsi::Array keys);
	jsi::Value getKeys(jsi::Runtime &rt, jsi::String dbName);
	jsi::Value clearStorage(jsi::Runtime &rt, jsi::String dbName);
	jsi::Value legacy_multiGet(jsi::Runtime &rt, jsi::Array keys);
	jsi::Value legacy_multiSet(jsi::Runtime &rt, jsi::Array kvPairs);
	jsi::Value legacy_multiRemove(jsi::Runtime &rt, jsi::Array keys);
	jsi::Value legacy_multiMerge(jsi::Runtime &rt, jsi::Array kvPairs);
	jsi::Value legacy_getAllKeys(jsi::Runtime &rt);
	jsi::Value legacy_clear(jsi::Runtime &rt);
};

} // namespace facebook::react
