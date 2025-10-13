#pragma once

#ifdef TOOLS_ENABLED

#include "editor/plugins/editor_plugin.h"

class ReactNativeFileEditorPlugin : public EditorPlugin {
	GDCLASS(ReactNativeFileEditorPlugin, EditorPlugin);

	String monitored_path;
	uint64_t last_known_modified_time = 0;
	bool last_known_exists = false;

	void _update_monitored_path();
	void _check_file_status(bool p_force_refresh = false);
	void _on_filesystem_changed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	ReactNativeFileEditorPlugin();
	~ReactNativeFileEditorPlugin();
};

#endif // TOOLS_ENABLED
