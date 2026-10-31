#pragma once

#ifdef TOOLS_ENABLED

#include "rn_node_process.h"
#include "rn_typescript_watcher.h"

#include "scene/gui/box_container.h"

class Button;
class CheckBox;
class HTTPRequest;
class ItemList;
class Label;
class TextEdit;

// The editor's React Native bottom panel: the Metro server the project uses,
// with its log and commands, and the TypeScript problems of the React project.
class RNEditorPanel : public VBoxContainer {
	GDCLASS(RNEditorPanel, VBoxContainer);

public:
	enum MetroState {
		METRO_OFF,
		METRO_PROBING,
		METRO_STARTING,
		METRO_RUNNING,
		METRO_ATTACHED,
		METRO_FAILED,
	};

private:
	MetroState state = METRO_OFF;
	RNNodeProcess metro;
	RNTypeScriptWatcher typescript;
	String server;
	double probe_delay = 0;
	String last_runtime_error;

	Label *status = nullptr;
	CheckBox *fast_refresh = nullptr;
	HTTPRequest *probe = nullptr;
	HTTPRequest *command = nullptr;
	TextEdit *log = nullptr;
	ItemList *problems = nullptr;
	Vector<RNProblem> listed_problems;

	void _settings_changed();
	void _start_metro();
	void _probe();
	void _probe_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _post(const String &p_path);
	void _restart();
	void _fast_refresh_toggled(bool p_enabled);
	void _process_metro(double p_delta);
	void _list_problems();
	void _problem_activated(int p_index);
	void _update_status();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	RNEditorPanel();
	MetroState get_metro_state() const { return state; }
};

#endif // TOOLS_ENABLED
