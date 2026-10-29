#pragma once

#ifdef TOOLS_ENABLED

#include "rn_source_map.h"

#include "editor/debugger/editor_debugger_plugin.h"
#include "scene/gui/box_container.h"

#include <map>

class HTTPRequest;
class TextEdit;
class Tree;
class TreeItem;

// One debug session's JavaScript errors, with source-mapped stacks, and its
// JavaScript console.
class RNDebuggerTab : public VBoxContainer {
	GDCLASS(RNDebuggerTab, VBoxContainer);

	struct CachedSourceMap {
		uint64_t modified_time = 0;
		std::shared_ptr<RNSourceMap> map;
	};

	Tree *errors = nullptr;
	TextEdit *console = nullptr;
	std::map<String, CachedSourceMap> source_maps;

	std::shared_ptr<RNSourceMap> _source_map(const String &p_bundle);
	void _show_frame(TreeItem *p_item, const String &p_method, const String &p_file, int p_line, int p_column);
	void _symbolicate_with_metro(TreeItem *p_error, const Array &p_frames, const String &p_server);
	void _metro_symbolicated(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, ObjectID p_error, HTTPRequest *p_request);
	void _frame_activated();

protected:
	static void _bind_methods() {}

public:
	RNDebuggerTab();
	void add_error(const String &p_message, const Array &p_frames);
	void add_log(int p_level, const String &p_message);
	void clear();
};

class RNDebuggerPlugin : public EditorDebuggerPlugin {
	GDCLASS(RNDebuggerPlugin, EditorDebuggerPlugin);

	HashMap<int, RNDebuggerTab *> tabs;

protected:
	static void _bind_methods() {}

public:
	bool has_capture(const String &p_capture) const override { return p_capture == "react_native"; }
	bool capture(const String &p_message, const Array &p_data, int p_session) override;
	void setup_session(int p_session) override;
};

#endif // TOOLS_ENABLED
