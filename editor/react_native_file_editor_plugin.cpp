#include "react_native_file_editor_plugin.h"

#ifdef TOOLS_ENABLED

#include "editor/file_system/editor_file_system.h"

#include "core/io/file_access.h"
#include "core/object/callable_method_pointer.h"

#include "../singletons/react_native_file_singleton.h"

void ReactNativeFileEditorPlugin::_bind_methods() {
}

ReactNativeFileEditorPlugin::ReactNativeFileEditorPlugin() {
	_update_monitored_path();
}

ReactNativeFileEditorPlugin::~ReactNativeFileEditorPlugin() {
	EditorFileSystem *fs = EditorFileSystem::get_singleton();
	if (fs && fs->is_connected(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeFileEditorPlugin::_on_filesystem_changed))) {
		fs->disconnect(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeFileEditorPlugin::_on_filesystem_changed));
	}
}

void ReactNativeFileEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorFileSystem *fs = EditorFileSystem::get_singleton();
			if (fs && !fs->is_connected(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeFileEditorPlugin::_on_filesystem_changed))) {
				fs->connect(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeFileEditorPlugin::_on_filesystem_changed));
			}
			_check_file_status(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			EditorFileSystem *fs = EditorFileSystem::get_singleton();
			if (fs && fs->is_connected(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeFileEditorPlugin::_on_filesystem_changed))) {
				fs->disconnect(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeFileEditorPlugin::_on_filesystem_changed));
			}
		} break;
	}
}

void ReactNativeFileEditorPlugin::_check_file_status(bool p_force_refresh) {
	_update_monitored_path();

	bool exists = !monitored_path.is_empty() && FileAccess::exists(monitored_path);
	uint64_t modified_time = exists ? FileAccess::get_modified_time(monitored_path) : 0;

	if (p_force_refresh || exists != last_known_exists || modified_time != last_known_modified_time) {
		last_known_exists = exists;
		last_known_modified_time = modified_time;

		ReactNativeFileSingleton *singleton = ReactNativeFileSingleton::get_singleton();
		if (singleton) {
			singleton->force_refresh();
		}
	}
}

void ReactNativeFileEditorPlugin::_update_monitored_path() {
	ReactNativeFileSingleton *singleton = ReactNativeFileSingleton::get_singleton();
	if (singleton) {
		monitored_path = singleton->get_monitored_file();
	} else {
		monitored_path = "res://test.txt";
	}
}

void ReactNativeFileEditorPlugin::_on_filesystem_changed() {
	_check_file_status();
}

#endif // TOOLS_ENABLED
