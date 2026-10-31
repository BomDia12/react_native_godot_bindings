#pragma once

#ifdef TOOLS_ENABLED

#include "editor/export/editor_export_plugin.h"

// Builds a release bundle into the export: the bundle file, which isn't a
// resource, and its assets, which are imported first. Source maps and build
// listings stay out.
class RNExportPlugin : public EditorExportPlugin {
	GDCLASS(RNExportPlugin, EditorExportPlugin);

	String output_directory;
	String project_root;

	bool _build(String &r_output);

protected:
	void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;

public:
	String get_name() const override { return "ReactNative"; }
	// Files that only tools read: what a build writes beside the bundle, and
	// the JavaScript project's own configuration, which Godot imports as JSON.
	static bool is_tooling_file(const String &p_path, const String &p_project_root, const String &p_output_directory);
};

#endif // TOOLS_ENABLED
