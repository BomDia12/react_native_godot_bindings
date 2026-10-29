#pragma once

#ifdef TOOLS_ENABLED

#include "rn_debugger_plugin.h"
#include "rn_editor_panel.h"
#include "rn_export_plugin.h"
#include "rn_javascript_highlighter.h"
#include "rn_root_inspector_plugin.h"

#include "editor/plugins/editor_plugin.h"

class ReactNativeEditorPlugin : public EditorPlugin {
	GDCLASS(ReactNativeEditorPlugin, EditorPlugin);

	String monitored_path;
	Ref<RNJavaScriptSyntaxHighlighter> highlighter;
	Ref<RNExportPlugin> export_plugin;
	Ref<RNDebuggerPlugin> debugger_plugin;
	Ref<RNRootInspectorPlugin> inspector_plugin;
	RNEditorPanel *panel = nullptr;
	uint64_t last_known_modified_time = 0;
	bool last_known_exists = false;

	void _ignore_node_modules();
	void _generate_contracts_and_quit();
	void _set_plugins_and_quit();
	void _update_monitored_path();
	void _check_file_status(bool p_force_refresh = false);
	void _on_filesystem_changed();
	void _open_javascript_files();
	void _resource_saved(const Ref<Resource> &p_resource);

protected:
	void _notification(int p_what);

public:
	ReactNativeEditorPlugin();
	~ReactNativeEditorPlugin();
};

#endif // TOOLS_ENABLED
