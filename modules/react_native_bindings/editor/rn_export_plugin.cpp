#include "rn_export_plugin.h"

#ifdef TOOLS_ENABLED

#include "../interop/rn_error.h"
#include "rn_contract_codegen.h"
#include "rn_tooling.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "editor/file_system/editor_file_system.h"

namespace {

void collect_files(const String &p_directory, Vector<String> &r_files) {
	for (const String &file : DirAccess::get_files_at(p_directory)) {
		r_files.push_back(p_directory.path_join(file));
	}
	for (const String &directory : DirAccess::get_directories_at(p_directory)) {
		collect_files(p_directory.path_join(directory), r_files);
	}
}

} // namespace

bool RNExportPlugin::is_tooling_file(const String &p_path, const String &p_project_root, const String &p_output_directory) {
	const String file = p_path.get_file();
	if (p_project_root.path_join(file) == p_path && (file == "package.json" || file == "package-lock.json" || file == "tsconfig.json")) {
		return true;
	}
	if (!p_path.begins_with(p_output_directory.path_join(""))) {
		return false;
	}
	return p_path.ends_with(".map") || p_path.ends_with(".dependencies.json") || p_path.get_file() == "assets-manifest.json";
}

bool RNExportPlugin::_build(String &r_output) {
	RNError error;
	if (!rn_generate_contracts(error)) {
		r_output = error.describe();
		return false;
	}
	List<String> arguments;
	arguments.push_back(rn_package_path("scripts/build.cjs"));
	arguments.push_back("--project-root");
	arguments.push_back(rn_project_root_path());
	int exit_code = -1;
	const Error started = OS::get_singleton()->execute(rn_node_path(), arguments, &r_output, &exit_code, true);
	return started == OK && exit_code == 0;
}

void RNExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	const String bundle = GLOBAL_GET("react_native/bundle/path");
	output_directory = bundle.get_base_dir();
	project_root = GLOBAL_GET("react_native/project/root");
	if (!rn_has_react_project()) {
		return;
	}
	String output;
	if (!_build(output)) {
		const String message = vformat("The React Native release bundle could not be built:\n%s", output);
		ERR_PRINT(message);
		if (get_export_platform().is_valid()) {
			get_export_platform()->add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, "React Native", message);
		}
		return;
	}
	// Export collects its files after this returns, so new assets must be
	// imported now to be included.
	Vector<String> files;
	collect_files(output_directory, files);
	EditorFileSystem::get_singleton()->update_files(files);
	Vector<String> assets;
	for (const String &file : files) {
		if (file != bundle && !is_tooling_file(file, project_root, output_directory)) {
			assets.push_back(file);
		}
	}
	EditorFileSystem::get_singleton()->reimport_files(assets);
	add_file(bundle, FileAccess::get_file_as_bytes(bundle), false);
	for (const String &entry : PackedStringArray(GLOBAL_GET("react_native/bundle/segments"))) {
		const String segment = output_directory.path_join(entry.get_file() + ".segment.js");
		if (!FileAccess::exists(segment)) {
			const String message = vformat("The React Native segment %s was not built.", segment);
			ERR_PRINT(message);
			if (get_export_platform().is_valid()) {
				get_export_platform()->add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, "React Native", message);
			}
			continue;
		}
		add_file(segment, FileAccess::get_file_as_bytes(segment), false);
	}
	_check_native_templates(p_debug);
}

// Export templates hold the engine a game runs on, so a library's native
// module reaches exported games only through templates built with it.
void RNExportPlugin::_check_native_templates(bool p_debug) {
	const String manifest = project_root.path_join("node_modules/.cache/react-native-godot/autolinking/autolinking.json");
	const Dictionary linked = FileAccess::exists(manifest) ? Dictionary(JSON::parse_string(FileAccess::get_file_as_string(manifest))) : Dictionary();
	const bool native = !Array(linked.get("modules", Array())).is_empty();
	const Ref<EditorExportPreset> preset = get_export_preset();
	if (native && preset.is_valid() && String(preset->get(p_debug ? "custom_template/debug" : "custom_template/release")).is_empty() && get_export_platform().is_valid()) {
		get_export_platform()->add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, "React Native", "The project links native Godot modules, which the default export templates lack. Use export templates built with RN_AUTOLINKING, as autolinking.md describes.");
	}
}

void RNExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	if (is_tooling_file(p_path, project_root, output_directory)) {
		skip();
	}
}

#endif // TOOLS_ENABLED
