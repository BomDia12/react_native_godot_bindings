#include "react_native_file_singleton.h"

#include "core/config/project_settings.h"
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

	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings) {
        configured_path = "res://test.txt";
	}

	force_refresh();
}

ReactNativeFileSingleton::~ReactNativeFileSingleton() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void ReactNativeFileSingleton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_monitored_file", "path"), &ReactNativeFileSingleton::set_monitored_file);
	ClassDB::bind_method(D_METHOD("get_monitored_file"), &ReactNativeFileSingleton::get_monitored_file);
	ClassDB::bind_method(D_METHOD("get_file_content"), &ReactNativeFileSingleton::get_file_content);
	ClassDB::bind_method(D_METHOD("has_file"), &ReactNativeFileSingleton::has_file);
	ClassDB::bind_method(D_METHOD("force_refresh"), &ReactNativeFileSingleton::force_refresh);

	ADD_SIGNAL(MethodInfo("react_native_file_changed", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::STRING, "content"), PropertyInfo(Variant::BOOL, "exists")));
}

void ReactNativeFileSingleton::set_monitored_file(const String &p_path) {
	if (configured_path == p_path) {
		return;
	}

	configured_path = p_path;
	last_modified_time = 0;

	_reload_content(true);
}

String ReactNativeFileSingleton::get_monitored_file() const {
	return configured_path;
}

String ReactNativeFileSingleton::get_file_content() const {
	return cached_content;
}

bool ReactNativeFileSingleton::has_file() const {
	return file_exists;
}

void ReactNativeFileSingleton::force_refresh() {
	_reload_content(true);
}

Error ReactNativeFileSingleton::_reload_content(bool p_force_emit) {
	bool path_valid = !configured_path.is_empty();
	bool exists = path_valid && FileAccess::exists(configured_path);

	if (!exists) {
		bool should_emit = p_force_emit || file_exists || !cached_content.is_empty();
		file_exists = false;
		cached_content = String();
		last_modified_time = 0;
		if (should_emit) {
			_emit_change();
		}
		return path_valid ? ERR_FILE_NOT_FOUND : ERR_INVALID_PARAMETER;
	}

	Error err = OK;
	String new_content = FileAccess::get_file_as_string(configured_path, &err);
	if (err != OK) {
		bool should_emit = p_force_emit || file_exists;
		file_exists = false;
		cached_content = String();
		last_modified_time = 0;
		if (should_emit) {
			_emit_change();
		}
		return err;
	}

	uint64_t modified_time = FileAccess::get_modified_time(configured_path);
	bool changed = p_force_emit || !file_exists || cached_content != new_content || last_modified_time != modified_time;

	cached_content = new_content;
	file_exists = true;
	last_modified_time = modified_time;

	if (changed) {
		_emit_change();
	}

	return OK;
}

void ReactNativeFileSingleton::_emit_change() {
	emit_signal("react_native_file_changed", configured_path, cached_content, file_exists);
}
