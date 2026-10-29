#include "rn_debugger_plugin.h"

#ifdef TOOLS_ENABLED

#include "rn_tooling.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "editor/debugger/script_editor_debugger.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/main/http_request.h"

// A long session keeps its most recent output, as the Metro log does.
constexpr int CONSOLE_LINES = 2000;
constexpr int ERRORS = 200;

RNDebuggerTab::RNDebuggerTab() {
	set_name("React Native");
	errors = memnew(Tree);
	errors->set_hide_root(true);
	errors->set_v_size_flags(SIZE_EXPAND_FILL);
	errors->connect("item_activated", callable_mp(this, &RNDebuggerTab::_frame_activated));
	errors->create_item();
	add_child(errors);
	console = memnew(TextEdit);
	console->set_editable(false);
	console->set_custom_minimum_size(Size2(0, 120));
	add_child(console);
}

void RNDebuggerTab::clear() {
	errors->clear();
	errors->create_item();
	console->set_text(String());
}

void RNDebuggerTab::add_log(int p_level, const String &p_message) {
	console->insert_line_at(console->get_line_count() - 1, p_message);
	if (console->get_line_count() > CONSOLE_LINES) {
		console->remove_text(0, 0, console->get_line_count() - CONSOLE_LINES, 0);
	}
}

void RNDebuggerTab::add_error(const String &p_message, const Array &p_frames) {
	if (errors->get_root()->get_child_count() >= ERRORS) {
		memdelete(errors->get_root()->get_first_child());
	}
	TreeItem *error = errors->create_item(errors->get_root());
	error->set_text(0, p_message.get_slicec('\n', 0));
	error->set_tooltip_text(0, p_message);
	String server;
	for (const Variant &entry : p_frames) {
		const Array frame = entry;
		const String file = frame[1];
		TreeItem *item = errors->create_item(error);
		_show_frame(item, frame[0], file, frame[2], frame[3]);
		RNSourcePosition position;
		const std::shared_ptr<RNSourceMap> map = file.begins_with("http") ? nullptr : _source_map(file);
		if (map && map->lookup(frame[2], frame[3], position)) {
			const String source = position.source.is_absolute_path() ? position.source : rn_project_root_path().path_join(position.source);
			_show_frame(item, position.name.is_empty() ? String(frame[0]) : position.name, source.simplify_path(), position.line, position.column);
		}
		if (file.begins_with("http") && server.is_empty()) {
			server = file.substr(0, file.find_char('/', file.find("//") + 2));
		}
	}
	if (!server.is_empty()) {
		_symbolicate_with_metro(error, p_frames, server);
	}
}

void RNDebuggerTab::_show_frame(TreeItem *p_item, const String &p_method, const String &p_file, int p_line, int p_column) {
	p_item->set_text(0, vformat("%s  %s:%d:%d", p_method, ProjectSettings::get_singleton()->localize_path(p_file), p_line, p_column));
	p_item->set_metadata(0, Dictionary({ { "file", p_file }, { "line", p_line }, { "column", p_column } }));
}

// Bundle files carry their source map beside them, as builds write it.
std::shared_ptr<RNSourceMap> RNDebuggerTab::_source_map(const String &p_bundle) {
	const String path = p_bundle + ".map";
	const uint64_t modified_time = FileAccess::exists(path) ? FileAccess::get_modified_time(path) : 0;
	CachedSourceMap &cached = source_maps[p_bundle];
	if (cached.modified_time == modified_time) {
		return cached.map;
	}
	cached.modified_time = modified_time;
	cached.map = std::make_shared<RNSourceMap>();
	if (modified_time == 0 || !cached.map->parse(FileAccess::get_file_as_string(path))) {
		cached.map = nullptr;
	}
	return cached.map;
}

void RNDebuggerTab::_symbolicate_with_metro(TreeItem *p_error, const Array &p_frames, const String &p_server) {
	Array stack;
	for (const Variant &entry : p_frames) {
		const Array frame = entry;
		stack.push_back(Dictionary({ { "methodName", frame[0] }, { "file", frame[1] }, { "lineNumber", frame[2] }, { "column", frame[3] } }));
	}
	HTTPRequest *request = memnew(HTTPRequest);
	add_child(request);
	request->connect("request_completed", callable_mp(this, &RNDebuggerTab::_metro_symbolicated).bind(p_error->get_instance_id(), request));
	request->request(p_server + "/symbolicate", { "Content-Type: application/json" }, HTTPClient::METHOD_POST, JSON::stringify(Dictionary({ { "stack", stack } })));
}

void RNDebuggerTab::_metro_symbolicated(int p_result, int p_code, const PackedStringArray &, const PackedByteArray &p_body, ObjectID p_error, HTTPRequest *p_request) {
	p_request->queue_free();
	// A restart or a newer error can remove the row before Metro answers.
	TreeItem *error = Object::cast_to<TreeItem>(ObjectDB::get_instance(p_error));
	const Variant response = JSON::parse_string(String::utf8(reinterpret_cast<const char *>(p_body.ptr()), p_body.size()));
	if (!error || p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200 || response.get_type() != Variant::DICTIONARY) {
		return;
	}
	const Array stack = Dictionary(response).get("stack", Array());
	TreeItem *item = error->get_first_child();
	for (int i = 0; i < stack.size() && item; ++i, item = item->get_next()) {
		const Dictionary frame = stack[i];
		_show_frame(item, frame.get("methodName", ""), frame.get("file", ""), frame.get("lineNumber", 0), frame.get("column", 0));
	}
}

void RNDebuggerTab::_frame_activated() {
	const Dictionary frame = errors->get_selected() ? Dictionary(errors->get_selected()->get_metadata(0)) : Dictionary();
	if (!frame.is_empty()) {
		rn_open_source(frame["file"], frame["line"], frame["column"]);
	}
}

bool RNDebuggerPlugin::capture(const String &p_message, const Array &p_data, int p_session) {
	RNDebuggerTab **tab = tabs.getptr(p_session);
	if (!tab) {
		return false;
	}
	if (p_message == "react_native:error" && p_data.size() == 2) {
		(*tab)->add_error(p_data[0], p_data[1]);
	} else if (p_message == "react_native:log" && p_data.size() == 2) {
		(*tab)->add_log(p_data[0], p_data[1]);
	}
	return true;
}

void RNDebuggerPlugin::setup_session(int p_session) {
	const Ref<EditorDebuggerSession> session = get_session(p_session);
	RNDebuggerTab *tab = memnew(RNDebuggerTab);
	session->add_session_tab(tab);
	session->connect("started", callable_mp(tab, &RNDebuggerTab::clear));
	tabs[p_session] = tab;
}

#endif // TOOLS_ENABLED
