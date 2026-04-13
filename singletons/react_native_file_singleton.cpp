#include "react_native_file_singleton.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/math/math_defs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/variant/variant.h"
#include "scene/main/scene_tree.h"

ReactNativeFileSingleton *ReactNativeFileSingleton::singleton = nullptr;

ReactNativeFileSingleton *ReactNativeFileSingleton::get_singleton() {
	return singleton;
}

ReactNativeFileSingleton::ReactNativeFileSingleton() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "ReactNativeFileSingleton is a singleton.");
	singleton = this;
	monitored_files = Vector<file_monitor_data *>({
		new file_monitor_data{ "index.js", 0, nullptr }
	});

	refresh(true);
}

ReactNativeFileSingleton::~ReactNativeFileSingleton() {
	if (singleton == this) {
		singleton = nullptr;
	}

	if (monitored_files.size() > 0) {
		for (int i = 0; i < monitored_files.size(); i++) {
			if (monitored_files[i]->processed_result != nullptr) {
				memdelete(monitored_files[i]->processed_result);
			}
			memdelete(monitored_files[i]);
		}
		monitored_files.clear();
	}
}

void ReactNativeFileSingleton::add_file_to_monitor(const String &p_path) {
	if (file_watched(p_path)) {
		return;
	}

	file_monitor_data *new_data = memnew(file_monitor_data);
	new_data->path = p_path;
	new_data->last_modified_time = 0;
	new_data->processed_result = nullptr;

	uint32_t changed = 0;

	_update_file_data(new_data, true, changed);

	monitored_files.push_back(new_data);
}

void ReactNativeFileSingleton::remove_file_from_monitor(const String &p_path) {
	for (int i = 0; i < monitored_files.size(); i++) {
		if (monitored_files[i]->path == p_path) {
			if (monitored_files[i]->processed_result != nullptr) {
				memdelete(monitored_files[i]->processed_result);
			}
			memdelete(monitored_files[i]);
			monitored_files.remove_at(i);
			return;
		}
	}
}

Vector<file_monitor_data *> ReactNativeFileSingleton::get_monitored_files() const {
	return monitored_files;
}

facebook::jsi::Object * ReactNativeFileSingleton::get_file_data(const String &p_path) const {
	for (int i = 0; i < monitored_files.size(); i++) {
		if (monitored_files[i]->path == p_path) {
			return monitored_files[i]->processed_result;
		}
	}
	return nullptr;
}

void ReactNativeFileSingleton::refresh(bool force = false) {
	_reload_data(force);
}

Error ReactNativeFileSingleton::_reload_data(bool p_force_emit) {
	Error err = OK;
	uint32_t changes = 0;

	for (int i = 0; i < monitored_files.size(); i++) {
		_update_file_data(monitored_files[i], p_force_emit, changes);
	}

	if (changes > 0) {
		_emit_change();
	}

	return OK;
}

Error ReactNativeFileSingleton::_update_file_data(file_monitor_data * p_data, bool p_force_emit, uint32_t &changed) {
	String path = base_path + p_data->path;
	
	bool file_exists = FileAccess::exists(path);

	if (!file_exists) {
		return ERR_FILE_NOT_FOUND;
	}

	uint64_t modified_time = OS::get_singleton()->get_unix_time();
	bool should_update = p_force_emit || (modified_time > p_data->last_modified_time);


	Error err = OK;
	String raw_file = FileAccess::get_file_as_string(path, &err);

	if (err != OK) {
		if (p_data->processed_result != nullptr) {
			memdelete(p_data->processed_result);
			p_data->processed_result = nullptr;
		}
		p_data->last_modified_time = 0;
		return err;
	}

	facebook::jsi::Object result = HermesRuntimeSingleton::get_singleton()->run_file(raw_file, path, err);
	if (err != OK) {
		if (p_data->processed_result != nullptr) {
			memdelete(p_data->processed_result);
			p_data->processed_result = nullptr;
		}
		p_data->last_modified_time = 0;
		return err;
	}

	p_data->last_modified_time = modified_time;
	if (p_data->processed_result != nullptr) {
		memdelete(p_data->processed_result);
	}
	p_data->processed_result = memnew(facebook::jsi::Object(std::move(result)));

	return OK;
}

bool ReactNativeFileSingleton::file_watched(const String &p_path) const {
	for (int i = 0; i < monitored_files.size(); i++) {
		if (monitored_files[i]->path == p_path) {
			return true;
		}
	}
	return false;
}

void ReactNativeFileSingleton::_emit_change() {
	emit_signal("react_native_file_changed");
}

void ReactNativeFileSingleton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh", "force"), &ReactNativeFileSingleton::refresh);
	ClassDB::bind_method(D_METHOD("add_file_to_monitor", "path"), &ReactNativeFileSingleton::add_file_to_monitor);
	ClassDB::bind_method(D_METHOD("remove_file_from_monitor", "path"), &ReactNativeFileSingleton::remove_file_from_monitor);
	ClassDB::bind_method(D_METHOD("get_monitored_files"), &ReactNativeFileSingleton::get_monitored_files);
	ClassDB::bind_method(D_METHOD("get_file_data", "path"), &ReactNativeFileSingleton::get_file_data);
	ClassDB::bind_method(D_METHOD("file_watched", "path"), &ReactNativeFileSingleton::file_watched);

	ADD_SIGNAL(MethodInfo("react_native_file_changed"));
}
