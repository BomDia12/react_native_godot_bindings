#include "rn_editor_panel.h"

#ifdef TOOLS_ENABLED

#include "../host/react_native_host.h"
#include "rn_tooling.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/main/http_request.h"
#include "servers/display/display_server.h"

namespace {

constexpr int LOG_LINES = 2000;
constexpr double PROBE_INTERVAL = 0.5;
constexpr double ATTACHED_PROBE_INTERVAL = 5;

} // namespace

RNEditorPanel::RNEditorPanel() {
	set_custom_minimum_size(Size2(0, 200));
	HBoxContainer *toolbar = memnew(HBoxContainer);
	add_child(toolbar);
	status = memnew(Label);
	status->set_h_size_flags(SIZE_EXPAND_FILL);
	toolbar->add_child(status);
	fast_refresh = memnew(CheckBox);
	fast_refresh->set_text("Fast Refresh");
	fast_refresh->connect(SceneStringName(toggled), callable_mp(this, &RNEditorPanel::_fast_refresh_toggled));
	toolbar->add_child(fast_refresh);
	const std::pair<const char *, Callable> buttons[] = {
		{ "Restart Metro", callable_mp(this, &RNEditorPanel::_restart) },
		{ "Reload", callable_mp(this, &RNEditorPanel::_post).bind("/reload") },
		{ "Open DevTools", callable_mp(this, &RNEditorPanel::_post).bind("/open-debugger") },
	};
	for (const auto &[label, action] : buttons) {
		Button *button = memnew(Button);
		button->set_text(label);
		button->connect(SceneStringName(pressed), action);
		toolbar->add_child(button);
	}

	TabContainer *tabs = memnew(TabContainer);
	tabs->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(tabs);
	log = memnew(TextEdit);
	log->set_name("Metro");
	log->set_editable(false);
	tabs->add_child(log);
	problems = memnew(ItemList);
	problems->set_name("Problems");
	problems->connect("item_activated", callable_mp(this, &RNEditorPanel::_problem_activated));
	tabs->add_child(problems);

	probe = memnew(HTTPRequest);
	probe->set_timeout(1);
	probe->connect("request_completed", callable_mp(this, &RNEditorPanel::_probe_completed));
	add_child(probe);
	command = memnew(HTTPRequest);
	add_child(command);
}

void RNEditorPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &RNEditorPanel::_settings_changed));
			if (rn_has_react_project()) {
				typescript.start();
			}
			_settings_changed();
			set_process(true);
		} break;
		case NOTIFICATION_PROCESS: {
			_process_metro(get_process_delta_time());
			if (typescript.poll()) {
				_list_problems();
			}
			const String runtime_error = ReactNativeHost::get_singleton() ? ReactNativeHost::get_singleton()->get_last_error() : String();
			if (runtime_error != last_runtime_error) {
				last_runtime_error = runtime_error;
				_list_problems();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			metro.stop();
			typescript.stop();
		} break;
	}
}

// A headless editor only runs tools, such as imports and exports, which
// shouldn't start a server that outlives them.
void RNEditorPanel::_settings_changed() {
	fast_refresh->set_pressed_no_signal(GLOBAL_GET("react_native/dev_server/fast_refresh"));
	const bool enabled = GLOBAL_GET("react_native/dev_server/enabled") && rn_has_react_project() && DisplayServer::get_singleton()->get_name() != "headless";
	const String next = enabled ? vformat("%s:%d", GLOBAL_GET("react_native/dev_server/host"), int(GLOBAL_GET("react_native/dev_server/port"))) : String();
	if (next == server) {
		return;
	}
	server = next;
	metro.stop();
	state = server.is_empty() ? METRO_OFF : METRO_PROBING;
	if (state == METRO_PROBING) {
		_probe();
	}
	_update_status();
}

void RNEditorPanel::_probe() {
	probe->cancel_request();
	probe->request("http://" + server + "/status");
}

// Metro answers its status probe once it serves; another project's server on
// the same port answers too, as it does for React Native's own tools.
void RNEditorPanel::_probe_completed(int p_result, int p_code, const PackedStringArray &, const PackedByteArray &p_body) {
	const bool running = p_result == HTTPRequest::RESULT_SUCCESS && p_code == 200 && String::utf8(reinterpret_cast<const char *>(p_body.ptr()), p_body.size()) == "packager-status:running";
	if (state == METRO_PROBING) {
		if (running) {
			state = METRO_ATTACHED;
		} else {
			_start_metro();
		}
	} else if (state == METRO_STARTING && running) {
		state = METRO_RUNNING;
	} else if (state == METRO_ATTACHED && p_result == HTTPRequest::RESULT_CANT_CONNECT) {
		// A busy Metro can be slow to answer, so only one that is gone is replaced.
		_start_metro();
	}
	_update_status();
}

void RNEditorPanel::_start_metro() {
	List<String> arguments;
	arguments.push_back("--project-root");
	arguments.push_back(rn_project_root_path());
	arguments.push_back("--host");
	arguments.push_back(GLOBAL_GET("react_native/dev_server/host"));
	arguments.push_back("--port");
	arguments.push_back(itos(int(GLOBAL_GET("react_native/dev_server/port"))));
	state = metro.start(rn_package_path("scripts/start.cjs"), arguments) ? METRO_STARTING : METRO_FAILED;
	probe_delay = PROBE_INTERVAL;
}

void RNEditorPanel::_process_metro(double p_delta) {
	const Vector<String> lines = metro.read_lines();
	for (const String &line : lines) {
		log->insert_line_at(log->get_line_count() - 1, line);
	}
	if (log->get_line_count() > LOG_LINES) {
		log->remove_text(0, 0, log->get_line_count() - LOG_LINES, 0);
	}
	if (!lines.is_empty()) {
		log->set_caret_line(log->get_line_count() - 1);
	}
	if ((state == METRO_STARTING || state == METRO_RUNNING) && !metro.is_running()) {
		state = METRO_FAILED;
		_update_status();
	} else if ((state == METRO_STARTING || state == METRO_ATTACHED) && (probe_delay -= p_delta) <= 0) {
		probe_delay = state == METRO_STARTING ? PROBE_INTERVAL : ATTACHED_PROBE_INTERVAL;
		_probe();
	}
}

void RNEditorPanel::_restart() {
	server = String();
	_settings_changed();
}

void RNEditorPanel::_post(const String &p_path) {
	if (state == METRO_RUNNING || state == METRO_ATTACHED) {
		command->cancel_request();
		command->request("http://" + server + p_path, PackedStringArray(), HTTPClient::METHOD_POST);
	}
}

void RNEditorPanel::_fast_refresh_toggled(bool p_enabled) {
	ProjectSettings::get_singleton()->set_setting("react_native/dev_server/fast_refresh", p_enabled);
	ProjectSettings::get_singleton()->save();
}

void RNEditorPanel::_list_problems() {
	problems->clear();
	listed_problems = typescript.get_problems();
	for (const RNProblem &problem : listed_problems) {
		problems->add_item(vformat("%s:%d:%d  %s", ProjectSettings::get_singleton()->localize_path(problem.file), problem.line, problem.column, problem.message));
	}
	if (!last_runtime_error.is_empty()) {
		problems->add_item("Editor preview: " + last_runtime_error);
	}
}

void RNEditorPanel::_problem_activated(int p_index) {
	if (p_index < listed_problems.size()) {
		const RNProblem &problem = listed_problems[p_index];
		rn_open_source(problem.file, problem.line, problem.column);
	}
}

void RNEditorPanel::_update_status() {
	const String states[] = {
		"Developer server off",
		"Looking for Metro on " + server,
		"Starting Metro on " + server,
		"Metro is running on " + server,
		"Using the Metro already running on " + server,
		vformat("Metro exited with code %d; see its log", metro.get_exit_code()),
	};
	status->set_text(states[state]);
}

#endif // TOOLS_ENABLED
