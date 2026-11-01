#include "rn_async_storage_module.h"

#include "../interop/rn_error.h"
#include "rn_js_promise.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"

namespace {

constexpr const char *STORAGE_DIRECTORY = "user://react_native/async_storage";
constexpr const char *STORAGE_ERROR = "AsyncStorageError";

String database_path(const String &p_name) {
	return vformat("%s/%s.json", STORAGE_DIRECTORY, p_name);
}

// In a directory of its own, so no database name can reach it.
String legacy_path() {
	return vformat("%s/legacy/storage.json", STORAGE_DIRECTORY);
}

} // namespace

String RNStorageFile::load() {
	if (loaded) {
		return String();
	}
	if (FileAccess::exists(path)) {
		Error error = OK;
		const String text = FileAccess::get_file_as_string(path, &error);
		if (error != OK) {
			return vformat("Could not read '%s'.", path);
		}
		const Variant parsed = JSON::parse_string(text);
		if (parsed.get_type() != Variant::DICTIONARY) {
			return vformat("'%s' is not a storage file.", path);
		}
		values = parsed;
	}
	loaded = true;
	return String();
}

String RNStorageFile::save() {
	const Error directory_error = DirAccess::make_dir_recursive_absolute(path.get_base_dir());
	if (directory_error != OK && directory_error != ERR_ALREADY_EXISTS) {
		return vformat("Could not create '%s'.", path.get_base_dir());
	}
	const String temporary = path + ".tmp";
	{
		Ref<FileAccess> file = FileAccess::open(temporary, FileAccess::WRITE);
		if (file.is_null()) {
			return vformat("Could not write '%s'.", temporary);
		}
		file->store_string(JSON::stringify(values, "", false));
	}
	return DirAccess::rename_absolute(temporary, path) == OK ? String() : vformat("Could not replace '%s'.", path);
}

RNStorageThread::~RNStorageThread() {
	{
		std::lock_guard lock(mutex);
		stopping = true;
	}
	wake.notify_all();
	if (thread.joinable()) {
		thread.join();
	}
}

void RNStorageThread::post(std::function<void()> p_task) {
	std::lock_guard lock(mutex);
	tasks.push_back(std::move(p_task));
	if (!thread.joinable()) {
		thread = std::thread([this] {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock wait_lock(mutex);
					wake.wait(wait_lock, [this] { return stopping || !tasks.empty(); });
					if (tasks.empty()) {
						return;
					}
					task = std::move(tasks.front());
					tasks.pop_front();
				}
				task();
			}
		});
	}
	wake.notify_one();
}

RNStorageFile &RNAsyncStorageState::file(const std::optional<String> &p_database) {
	const String path = p_database ? database_path(*p_database) : legacy_path();
	std::unique_ptr<RNStorageFile> &found = databases[path.utf8().get_data()];
	if (!found) {
		found = std::make_unique<RNStorageFile>(path);
	}
	return *found;
}

bool rn_storage_name_is_valid(const String &p_name) {
	if (p_name.is_empty() || p_name.begins_with(".")) {
		return false;
	}
	for (int i = 0; i < p_name.length(); ++i) {
		const char32_t c = p_name[i];
		if (!is_ascii_alphanumeric_char(c) && c != '_' && c != '-' && c != '.') {
			return false;
		}
	}
	return true;
}

Dictionary rn_storage_merge(const Dictionary &p_target, const Dictionary &p_source) {
	Dictionary merged = p_target.duplicate();
	for (const KeyValue<Variant, Variant> &entry : p_source) {
		const Variant existing = merged.get(entry.key, Variant());
		if (existing.get_type() == Variant::DICTIONARY && entry.value.get_type() == Variant::DICTIONARY) {
			merged[entry.key] = rn_storage_merge(existing, entry.value);
		} else {
			merged[entry.key] = entry.value;
		}
	}
	return merged;
}

namespace facebook::react {

namespace {

String to_godot(jsi::Runtime &rt, const jsi::String &p_string) {
	return String::utf8(p_string.utf8(rt).c_str());
}

std::vector<String> strings_from_js(jsi::Runtime &rt, const jsi::Array &p_array) {
	std::vector<String> strings;
	for (size_t i = 0; i < p_array.size(rt); ++i) {
		strings.push_back(to_godot(rt, p_array.getValueAtIndex(rt, i).asString(rt)));
	}
	return strings;
}

jsi::Value js_string(jsi::Runtime &rt, const Variant &p_value) {
	return jsi::String::createFromUtf8(rt, String(p_value).utf8().get_data());
}

RNJSValueFactory no_value() {
	return [](jsi::Runtime &) { return jsi::Value::undefined(); };
}

RNJSValueFactory entries_value(std::vector<std::pair<String, Variant>> p_entries) {
	return [entries = std::move(p_entries)](jsi::Runtime &rt) -> jsi::Value {
		jsi::Array result(rt, entries.size());
		for (size_t i = 0; i < entries.size(); ++i) {
			jsi::Object entry(rt);
			entry.setProperty(rt, "key", js_string(rt, entries[i].first));
			entry.setProperty(rt, "value", entries[i].second.get_type() == Variant::NIL ? jsi::Value::null() : js_string(rt, entries[i].second));
			result.setValueAtIndex(rt, i, std::move(entry));
		}
		return result;
	};
}

RNJSValueFactory keys_value(const Dictionary &p_values) {
	return [keys = p_values.keys()](jsi::Runtime &rt) -> jsi::Value {
		jsi::Array result(rt, keys.size());
		for (int i = 0; i < keys.size(); ++i) {
			result.setValueAtIndex(rt, i, js_string(rt, keys[i]));
		}
		return result;
	};
}

} // namespace

// Runs p_work on the storage thread once the database is loaded, then settles
// the promise with what p_work returns. No database name means the legacy one.
template <typename Work>
jsi::Value RNAsyncStorageModule::_run(jsi::Runtime &rt, const std::optional<String> &p_database, Work p_work) {
	if (p_database && !rn_storage_name_is_valid(*p_database)) {
		const std::string message = vformat("'%s' is not a valid database name; use letters, numbers, '_', '-' and '.'.", *p_database).utf8().get_data();
		return RNJSPromise::create(rt, jsInvoker_, [&message](std::shared_ptr<RNJSPromise> p_promise) { p_promise->reject(RNErrorCode::VALIDATION, message); });
	}
	return RNJSPromise::create(rt, jsInvoker_, [state = storage.get(), p_database, work = std::move(p_work)](std::shared_ptr<RNJSPromise> p_promise) {
		state->thread.post([state, p_database, work, p_promise] {
			RNStorageFile &file = state->file(p_database);
			String error = file.load();
			RNJSValueFactory result;
			if (error.is_empty()) {
				result = work(file, error);
			}
			if (error.is_empty()) {
				p_promise->resolve(result);
			} else {
				p_promise->reject(STORAGE_ERROR, error.utf8().get_data());
			}
		});
	});
}

RNAsyncStorageModule::RNAsyncStorageModule(std::shared_ptr<CallInvoker> jsInvoker, std::shared_ptr<RNAsyncStorageState> p_storage) :
		NativeAsyncStorageCxxSpec(std::move(jsInvoker)), storage(std::move(p_storage)) {
}

jsi::Value RNAsyncStorageModule::getValues(jsi::Runtime &rt, jsi::String dbName, jsi::Array keys) {
	return _run(rt, to_godot(rt, dbName), [keys = strings_from_js(rt, keys)](RNStorageFile &p_file, String &) {
		std::vector<std::pair<String, Variant>> entries;
		for (const String &key : keys) {
			entries.emplace_back(key, p_file.get_values().get(key, Variant()));
		}
		return entries_value(std::move(entries));
	});
}

jsi::Value RNAsyncStorageModule::setValues(jsi::Runtime &rt, jsi::String dbName, jsi::Array values) {
	std::vector<std::pair<String, Variant>> entries;
	for (size_t i = 0; i < values.size(rt); ++i) {
		const jsi::Object entry = values.getValueAtIndex(rt, i).asObject(rt);
		const jsi::Value value = entry.getProperty(rt, "value");
		entries.emplace_back(to_godot(rt, entry.getProperty(rt, "key").asString(rt)), value.isString() ? Variant(to_godot(rt, value.asString(rt))) : Variant());
	}
	return _run(rt, to_godot(rt, dbName), [entries = std::move(entries)](RNStorageFile &p_file, String &r_error) {
		for (const auto &[key, value] : entries) {
			if (value.get_type() == Variant::NIL) {
				p_file.get_values().erase(key);
			} else {
				p_file.get_values()[key] = value;
			}
		}
		r_error = p_file.save();
		return entries_value(entries);
	});
}

jsi::Value RNAsyncStorageModule::removeValues(jsi::Runtime &rt, jsi::String dbName, jsi::Array keys) {
	return _run(rt, to_godot(rt, dbName), [keys = strings_from_js(rt, keys)](RNStorageFile &p_file, String &r_error) {
		for (const String &key : keys) {
			p_file.get_values().erase(key);
		}
		r_error = p_file.save();
		return no_value();
	});
}

jsi::Value RNAsyncStorageModule::getKeys(jsi::Runtime &rt, jsi::String dbName) {
	return _run(rt, to_godot(rt, dbName), [](RNStorageFile &p_file, String &) { return keys_value(p_file.get_values()); });
}

jsi::Value RNAsyncStorageModule::clearStorage(jsi::Runtime &rt, jsi::String dbName) {
	return _run(rt, to_godot(rt, dbName), [](RNStorageFile &p_file, String &r_error) {
		p_file.get_values().clear();
		r_error = p_file.save();
		return no_value();
	});
}

jsi::Value RNAsyncStorageModule::legacy_multiGet(jsi::Runtime &rt, jsi::Array keys) {
	return _run(rt, std::nullopt, [keys = strings_from_js(rt, keys)](RNStorageFile &p_file, String &) -> RNJSValueFactory {
		std::vector<std::pair<String, Variant>> entries;
		for (const String &key : keys) {
			entries.emplace_back(key, p_file.get_values().get(key, Variant()));
		}
		return [entries = std::move(entries)](jsi::Runtime &p_runtime) -> jsi::Value {
			jsi::Array result(p_runtime, entries.size());
			for (size_t i = 0; i < entries.size(); ++i) {
				jsi::Array pair(p_runtime, 2);
				pair.setValueAtIndex(p_runtime, 0, js_string(p_runtime, entries[i].first));
				pair.setValueAtIndex(p_runtime, 1, entries[i].second.get_type() == Variant::NIL ? jsi::Value::null() : js_string(p_runtime, entries[i].second));
				result.setValueAtIndex(p_runtime, i, std::move(pair));
			}
			return result;
		};
	});
}

jsi::Value RNAsyncStorageModule::legacy_multiSet(jsi::Runtime &rt, jsi::Array kvPairs) {
	std::vector<std::pair<String, String>> pairs;
	for (size_t i = 0; i < kvPairs.size(rt); ++i) {
		const jsi::Array pair = kvPairs.getValueAtIndex(rt, i).asObject(rt).asArray(rt);
		pairs.emplace_back(to_godot(rt, pair.getValueAtIndex(rt, 0).asString(rt)), to_godot(rt, pair.getValueAtIndex(rt, 1).asString(rt)));
	}
	return _run(rt, std::nullopt, [pairs = std::move(pairs)](RNStorageFile &p_file, String &r_error) {
		for (const auto &[key, value] : pairs) {
			p_file.get_values()[key] = value;
		}
		r_error = p_file.save();
		return no_value();
	});
}

jsi::Value RNAsyncStorageModule::legacy_multiRemove(jsi::Runtime &rt, jsi::Array keys) {
	return _run(rt, std::nullopt, [keys = strings_from_js(rt, keys)](RNStorageFile &p_file, String &r_error) {
		for (const String &key : keys) {
			p_file.get_values().erase(key);
		}
		r_error = p_file.save();
		return no_value();
	});
}

jsi::Value RNAsyncStorageModule::legacy_multiMerge(jsi::Runtime &rt, jsi::Array kvPairs) {
	std::vector<std::pair<String, String>> pairs;
	for (size_t i = 0; i < kvPairs.size(rt); ++i) {
		const jsi::Array pair = kvPairs.getValueAtIndex(rt, i).asObject(rt).asArray(rt);
		pairs.emplace_back(to_godot(rt, pair.getValueAtIndex(rt, 0).asString(rt)), to_godot(rt, pair.getValueAtIndex(rt, 1).asString(rt)));
	}
	return _run(rt, std::nullopt, [pairs = std::move(pairs)](RNStorageFile &p_file, String &r_error) {
		for (const auto &[key, value] : pairs) {
			const Variant next = JSON::parse_string(value);
			const Variant current = JSON::parse_string(String(p_file.get_values().get(key, String())));
			if (next.get_type() == Variant::DICTIONARY && current.get_type() == Variant::DICTIONARY) {
				p_file.get_values()[key] = JSON::stringify(rn_storage_merge(current, next), "", false);
			} else {
				p_file.get_values()[key] = value;
			}
		}
		r_error = p_file.save();
		return no_value();
	});
}

jsi::Value RNAsyncStorageModule::legacy_getAllKeys(jsi::Runtime &rt) {
	return _run(rt, std::nullopt, [](RNStorageFile &p_file, String &) { return keys_value(p_file.get_values()); });
}

jsi::Value RNAsyncStorageModule::legacy_clear(jsi::Runtime &rt) {
	return _run(rt, std::nullopt, [](RNStorageFile &p_file, String &r_error) {
		p_file.get_values().clear();
		r_error = p_file.save();
		return no_value();
	});
}

} // namespace facebook::react
