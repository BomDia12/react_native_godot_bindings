#include "react_native_editor_plugin.h"

#ifdef TOOLS_ENABLED

#include "../singletons/react_native_file_singleton.h"
#include "rn_contract_codegen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/main/scene_tree.h"

ReactNativeEditorPlugin::ReactNativeEditorPlugin() {
	EDITOR_DEF("react_native/tooling/node_path", "node");
	_ignore_node_modules();
	_update_monitored_path();
	_open_javascript_files();
	highlighter.instantiate();
	export_plugin.instantiate();
	add_export_plugin(export_plugin);
	debugger_plugin.instantiate();
	add_debugger_plugin(debugger_plugin);
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
	if (!rn_is_generating_contracts()) {
		panel = memnew(RNEditorPanel);
		add_control_to_bottom_panel(panel, "React Native");
	}
}

// Godot's script editor opens files whose extensions this per-user setting lists.
void ReactNativeEditorPlugin::_open_javascript_files() {
	PackedStringArray extensions = String(EDITOR_GET("docks/filesystem/textfile_extensions")).split(",", false);
	for (const String &extension : RNJavaScriptSyntaxHighlighter::extensions()) {
		if (!extensions.has(extension)) {
			extensions.push_back(extension);
		}
	}
	EditorSettings::get_singleton()->set_setting("docks/filesystem/textfile_extensions", String(",").join(extensions));
}

void ReactNativeEditorPlugin::_resource_saved(const Ref<Resource> &p_resource) {
	if (Object::cast_to<RNComponentContract>(p_resource.ptr())) {
		RNError error;
		if (!rn_generate_contracts(error)) {
			ERR_PRINT(vformat("Generating React Native contracts failed: %s", error.describe()));
		}
	}
}

ReactNativeEditorPlugin::~ReactNativeEditorPlugin() {
	EditorFileSystem *fs = EditorFileSystem::get_singleton();
	if (fs && fs->is_connected(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeEditorPlugin::_on_filesystem_changed))) {
		fs->disconnect(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeEditorPlugin::_on_filesystem_changed));
	}
}

void ReactNativeEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorFileSystem *fs = EditorFileSystem::get_singleton();
			if (fs && !fs->is_connected(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeEditorPlugin::_on_filesystem_changed))) {
				fs->connect(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeEditorPlugin::_on_filesystem_changed));
			}
			_check_file_status(true);
			ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);
			connect(SNAME("resource_saved"), callable_mp(this, &ReactNativeEditorPlugin::_resource_saved));
			if (rn_is_generating_contracts()) {
				_generate_contracts_and_quit();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			ScriptEditor::get_singleton()->unregister_syntax_highlighter(highlighter);
			disconnect(SNAME("resource_saved"), callable_mp(this, &ReactNativeEditorPlugin::_resource_saved));
			EditorFileSystem *fs = EditorFileSystem::get_singleton();
			if (fs && fs->is_connected(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeEditorPlugin::_on_filesystem_changed))) {
				fs->disconnect(SNAME("filesystem_changed"), callable_mp(this, &ReactNativeEditorPlugin::_on_filesystem_changed));
			}
		} break;
	}
}

void ReactNativeEditorPlugin::_check_file_status(bool p_force_refresh) {
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

// `godot --headless --editor --react-native-generate-contracts` generates the
// TypeScript for the project's contracts, as CLI builds and CI need.
void ReactNativeEditorPlugin::_generate_contracts_and_quit() {
	// Quitting during the first scan aborts it with a warning.
	if (EditorFileSystem::get_singleton()->is_scanning()) {
		get_tree()->connect(SNAME("process_frame"), callable_mp(this, &ReactNativeEditorPlugin::_generate_contracts_and_quit), CONNECT_ONE_SHOT);
		return;
	}
	RNError error;
	const bool generated = rn_generate_contracts(error);
	if (!generated) {
		ERR_PRINT(vformat("Generating React Native contracts failed: %s", error.describe()));
	}
	get_tree()->quit(generated ? 0 : 1);
}

void ReactNativeEditorPlugin::_update_monitored_path() {
	ReactNativeFileSingleton *singleton = ReactNativeFileSingleton::get_singleton();
	monitored_path = singleton ? singleton->get_monitored_file() : String();
}

// Godot would otherwise import, and export, the thousands of files that npm
// installs, while the React sources beside them stay visible to the editor.
void ReactNativeEditorPlugin::_ignore_node_modules() {
	const String node_modules = String(GLOBAL_GET("react_native/project/root")).path_join("node_modules");
	const String marker = node_modules.path_join(".gdignore");
	if (DirAccess::exists(node_modules) && !FileAccess::exists(marker)) {
		FileAccess::open(marker, FileAccess::WRITE);
	}
}

void ReactNativeEditorPlugin::_on_filesystem_changed() {
	_ignore_node_modules();
	_check_file_status();
}

#endif // TOOLS_ENABLED
