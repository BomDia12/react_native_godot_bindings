#pragma once

#include "scene/gui/line_edit.h"
#include "scene/gui/text_edit.h"
#include "scene/resources/font.h"

#include <functional>
#include <memory>
#include <optional>

class RNLineEditor : public LineEdit {
	GDCLASS(RNLineEditor, LineEdit);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};
class RNTextEditor : public TextEdit {
	GDCLASS(RNTextEditor, TextEdit);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
};
struct RNTextInputConfig {
	Ref<Font> font;
	int font_size = 16;
	Color font_color = Color(1, 1, 1);
	std::optional<Color> selection_color;
	std::optional<Color> cursor_color;
	std::optional<Color> placeholder_color;
	bool editable = true;
	String placeholder;
	bool secure = false;
	String submit_behavior;
	bool multiline = false;
	int max_length = 0;
	bool auto_focus = false;
	bool targetable = true;
};
class RNTextInputControl : public Control {
	GDCLASS(RNTextInputControl, Control);
	Control *editor = nullptr;
	Control *staged_editor = nullptr;
	RNTextInputConfig config;
	std::function<void(const StringName &, const Dictionary &)> event_callback;
	std::function<void()> layout_callback;
	Dictionary deferred_replacement;
	bool changing = false;
	bool published = false;
	bool auto_focus_pending = false;
	bool pending_native_event = false;
	int event_count = 0;
	uint64_t text_revision = UINT64_MAX;
	uint64_t selection_revision = UINT64_MAX;
	String observed_text;
	Point2i observed_selection;
	Ref<Font> subscribed_font;
	void _native_changed();
	void _line_changed(const String &p_text);
	void _selection_changed();
	void _focus_entered();
	void _focus_exited();
	void _submitted(const String &p_text);
	void _editor_input(const Ref<InputEvent> &p_event);
	void _font_changed();
	void _auto_focus();
	Control *_create_editor(bool p_multiline);
	void _configure_editor(Control *p_editor);
	void _swap_editor();
	String _submit_behavior() const;
	void _emit(const StringName &p_event, Dictionary p_payload = Dictionary());
	void _set_text(const String &p_text);
	void _set_selection(int p_start, int p_end);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNTextInputControl();
	~RNTextInputControl();
	Control *get_editor() const { return editor; }
	String get_text() const;
	Point2i get_selection() const;
	bool is_composing() const;
	int get_event_count() const { return event_count; }
	void set_callbacks(std::function<void(const StringName &, const Dictionary &)> p_event, std::function<void()> p_layout);
	void configure(const RNTextInputConfig &p_config);
	void reconcile(int p_ack, const Variant &p_text, int p_start, int p_end);
	void focus();
	void blur();
};
