#include "rn_tooling.h"

#ifdef TOOLS_ENABLED

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_settings.h"

String rn_node_path() {
	return EDITOR_GET("react_native/tooling/node_path");
}

String rn_project_root_path() {
	return ProjectSettings::get_singleton()->globalize_path(GLOBAL_GET("react_native/project/root"));
}

String rn_package_path(const String &p_relative) {
	return rn_project_root_path().path_join("node_modules/react-native-godot").path_join(p_relative);
}

bool rn_has_react_project() {
	return FileAccess::exists(rn_package_path("package.json"));
}

bool rn_open_source(const String &p_path, int p_line, int p_column) {
	const String path = ProjectSettings::get_singleton()->localize_path(p_path);
	if (!path.begins_with("res://") || !FileAccess::exists(path)) {
		return false;
	}
	const Ref<Resource> file = ScriptEditor::get_singleton()->open_file(path);
	return file.is_valid() && ScriptEditor::get_singleton()->edit(file, MAX(p_line - 1, 0), MAX(p_column - 1, 0));
}

#endif // TOOLS_ENABLED
