#include "rn_text_input_control.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "scene/main/viewport.h"

namespace {
int utf16_offset(const String &p_text, int p_native) {
	int count = 0;
	for (int i = 0; i < MIN(p_native, p_text.length()); ++i) {
		count += p_text[i] > 0xffff ? 2 : 1;
	}
	return count;
}
int native_offset(const String &p_text, int p_utf16) {
	int units = 0;
	int index = 0;
	while (index < p_text.length() && units < p_utf16) {
		int next = units + (p_text[index] > 0xffff ? 2 : 1);
		if (next > p_utf16) {
			break;
		}
		units = next;
		++index;
	}
	return index;
}
Point2i line_column(const String &p_text, int p_index) {
	Point2i result;
	for (int i = 0; i < MIN(p_index, p_text.length()); ++i) {
		if (p_text[i] == '\n') {
			++result.y;
			result.x = 0;
		} else {
			++result.x;
		}
	}
	return result;
}
int flat_offset(TextEdit *p_editor, int p_line, int p_column) {
	int index = p_column;
	for (int i = 0; i < p_line; ++i) {
		index += p_editor->get_line(i).length() + 1;
	}
	return index;
}
} //namespace
RNTextInputControl::RNTextInputControl() {
	set_mouse_filter(MOUSE_FILTER_PASS);
	editor = _create_editor(false);
	add_child(editor);
	set_process(true);
}
RNTextInputControl::~RNTextInputControl() {
	if (subscribed_font.is_valid()) {
		subscribed_font->disconnect("changed", callable_mp(this, &RNTextInputControl::_font_changed));
	}
}
Control *RNTextInputControl::_create_editor(bool p_multiline) {
	Control *result;
	if (p_multiline) {
		auto *edit = memnew(RNTextEditor);
		edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
		edit->connect("text_changed", callable_mp(this, &RNTextInputControl::_native_changed));
		edit->connect("caret_changed", callable_mp(this, &RNTextInputControl::_selection_changed));
		result = edit;
	} else {
		auto *line = memnew(RNLineEditor);
		line->connect("text_changed", callable_mp(this, &RNTextInputControl::_line_changed));
		line->connect("text_submitted", callable_mp(this, &RNTextInputControl::_submitted));
		result = line;
	}
	result->connect("focus_entered", callable_mp(this, &RNTextInputControl::_focus_entered));
	result->connect("focus_exited", callable_mp(this, &RNTextInputControl::_focus_exited));
	result->connect("gui_input", callable_mp(this, &RNTextInputControl::_editor_input));
	return result;
}
String RNTextInputControl::get_text() const {
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		return line->get_text();
	}
	return Object::cast_to<TextEdit>(editor)->get_text();
}
Point2i RNTextInputControl::get_selection() const {
	const String text = get_text();
	int start;
	int end;
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		start = line->has_selection() ? line->get_selection_from_column() : line->get_caret_column();
		end = line->has_selection() ? line->get_selection_to_column() : start;
	} else {
		auto *edit = Object::cast_to<TextEdit>(editor);
		start = flat_offset(edit, edit->has_selection() ? edit->get_selection_from_line() : edit->get_caret_line(), edit->has_selection() ? edit->get_selection_from_column() : edit->get_caret_column());
		end = edit->has_selection() ? flat_offset(edit, edit->get_selection_to_line(), edit->get_selection_to_column()) : start;
	}
	return Point2i(utf16_offset(text, start), utf16_offset(text, end));
}
bool RNTextInputControl::is_composing() const {
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		return line->has_ime_text();
	}
	return Object::cast_to<TextEdit>(editor)->has_ime_text();
}
void RNTextInputControl::_set_text(const String &p_text) {
	if (p_text == get_text()) {
		return;
	}
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		line->set_text(p_text);
	} else {
		Object::cast_to<TextEdit>(editor)->set_text(p_text);
	}
	observed_text = get_text();
}
void RNTextInputControl::_set_selection(int p_start, int p_end) {
	if (p_start < 0 || p_end < 0) {
		return;
	}
	const String text = get_text();
	int start = native_offset(text, MIN(p_start, p_end));
	int end = native_offset(text, MAX(p_start, p_end));
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		line->set_caret_column(end);
		if (start == end) {
			line->deselect();
		} else {
			line->select(start, end);
		}
	} else {
		auto *edit = Object::cast_to<TextEdit>(editor);
		Point2i from = line_column(text, start);
		Point2i to = line_column(text, end);
		edit->set_caret_line(to.y);
		edit->set_caret_column(to.x);
		if (start == end) {
			edit->deselect();
		} else {
			edit->select(from.y, from.x, to.y, to.x);
		}
	}
	observed_selection = get_selection();
}
void RNTextInputControl::reconcile(int p_ack, const Variant &p_text, int p_start, int p_end) {
	if (!is_composing() && get_text() != observed_text) {
		_native_changed();
	}
	if (p_ack != event_count) {
		return;
	}
	if (is_composing()) {
		deferred_replacement["ack"] = p_ack;
		deferred_replacement["text"] = p_text;
		deferred_replacement["start"] = p_start;
		deferred_replacement["end"] = p_end;
		return;
	}
	deferred_replacement.clear();
	changing = true;
	if (p_text.get_type() == Variant::STRING) {
		_set_text(p_text);
	}
	_set_selection(p_start, p_end);
	changing = false;
}
String RNTextInputControl::_submit_behavior() const {
	if (!config.submit_behavior.is_empty()) {
		return config.submit_behavior;
	}
	return config.multiline ? "newline" : "blurAndSubmit";
}
void RNTextInputControl::_configure_editor(Control *p_editor) {
	p_editor->set_mouse_filter(get_mouse_filter() == MOUSE_FILTER_IGNORE ? MOUSE_FILTER_IGNORE : MOUSE_FILTER_STOP);
	if (config.font.is_valid()) {
		p_editor->add_theme_font_override("font", config.font);
	} else {
		p_editor->remove_theme_font_override("font");
	}
	p_editor->add_theme_font_size_override("font_size", config.font_size);
	p_editor->add_theme_color_override("font_color", config.font_color);
	const std::pair<const char *, std::optional<Color>> colors[] = { { "selection_color", config.selection_color }, { "caret_color", config.cursor_color }, { "font_placeholder_color", config.placeholder_color } };
	for (const auto &[name, color] : colors) {
		if (color) {
			p_editor->add_theme_color_override(name, *color);
		} else {
			p_editor->remove_theme_color_override(name);
		}
	}
	if (auto *line = Object::cast_to<LineEdit>(p_editor)) {
		line->set_editable(config.editable);
		line->set_placeholder(config.placeholder);
		line->set_secret(config.secure);
		line->set_keep_editing_on_text_submit(_submit_behavior() == "submit");
	} else {
		auto *edit = Object::cast_to<TextEdit>(p_editor);
		edit->set_editable(config.editable);
		edit->set_placeholder(config.placeholder);
	}
	p_editor->set_size(get_size());
}
void RNTextInputControl::set_callbacks(std::function<void(const StringName &, const Dictionary &)> p_event, std::function<void()> p_layout) {
	event_callback = std::move(p_event);
	layout_callback = std::move(p_layout);
}
void RNTextInputControl::_swap_editor() {
	const bool was_focused = editor->has_focus();
	const String text = get_text();
	const Point2i selection = get_selection();
	changing = true;
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		line->cancel_ime();
	} else {
		Object::cast_to<TextEdit>(editor)->cancel_ime();
	}
	memdelete(editor);
	editor = staged_editor;
	staged_editor = nullptr;
	editor->show();
	deferred_replacement.clear();
	_set_text(text);
	_set_selection(selection.x, selection.y);
	changing = false;
	if (auto *edit = Object::cast_to<TextEdit>(editor)) {
		edit->clear_undo_history();
	}
	if (was_focused) {
		focus();
	}
}
void RNTextInputControl::configure(const RNTextInputConfig &p_config) {
	changing = true;
	config = p_config;
	set_mouse_filter(config.targetable ? MOUSE_FILTER_PASS : MOUSE_FILTER_IGNORE);
	if (config.multiline != (Object::cast_to<TextEdit>(editor) != nullptr)) {
		staged_editor = _create_editor(config.multiline);
		add_child(staged_editor);
		changing = false;
		_swap_editor();
		changing = true;
	}
	_configure_editor(editor);
	changing = false;
	if (subscribed_font != config.font) {
		if (subscribed_font.is_valid()) {
			subscribed_font->disconnect("changed", callable_mp(this, &RNTextInputControl::_font_changed));
		}
		subscribed_font = config.font;
		if (subscribed_font.is_valid()) {
			subscribed_font->connect("changed", callable_mp(this, &RNTextInputControl::_font_changed));
		}
	}
	if (!published) {
		published = true;
		auto_focus_pending = config.auto_focus;
		if (is_inside_tree()) {
			_auto_focus();
		}
	}
}
// The first configuration usually happens before mounting adds the input to
// the tree, so autofocus waits for it.
void RNTextInputControl::_auto_focus() {
	if (auto_focus_pending && is_visible_in_tree()) {
		focus();
	}
	auto_focus_pending = false;
}
void RNTextInputControl::_emit(const StringName &p_event, Dictionary p_payload) {
	if (!changing && event_callback) {
		p_payload["text"] = get_text();
		p_payload["eventCount"] = event_count;
		event_callback(p_event, p_payload);
	}
}
void RNTextInputControl::_line_changed(const String &) {
	_native_changed();
}
void RNTextInputControl::_native_changed() {
	if (is_composing() || get_text() == observed_text) {
		return;
	}
	const int maximum = config.max_length;
	const String text = get_text();
	if (maximum > 0 && utf16_offset(text, text.length()) > maximum) {
		changing = true;
		const int end = native_offset(text, maximum);
		if (auto *line = Object::cast_to<LineEdit>(editor)) {
			line->delete_text(end, text.length());
		} else {
			auto *edit = Object::cast_to<TextEdit>(editor);
			Point2i from = line_column(text, end);
			Point2i to = line_column(text, text.length());
			edit->remove_text(from.y, from.x, to.y, to.x);
		}
		changing = false;
	}
	observed_text = get_text();
	++event_count;
	deferred_replacement.clear();
	if (changing) {
		pending_native_event = true;
	} else {
		pending_native_event = false;
		_emit("topChange");
	}
	_selection_changed();
}
void RNTextInputControl::_selection_changed() {
	if (changing || is_composing()) {
		return;
	}
	const Point2i range = get_selection();
	if (range == observed_selection) {
		return;
	}
	observed_selection = range;
	Dictionary selection;
	selection["start"] = range.x;
	selection["end"] = range.y;
	Dictionary event;
	event["selection"] = selection;
	_emit("topSelectionChange", event);
}
void RNTextInputControl::_submitted(const String &) {
	_emit("topSubmitEditing");
	if (_submit_behavior() == "blurAndSubmit") {
		blur();
	}
}
void RNTextInputControl::_focus_entered() {
	_emit("topFocus");
}
void RNTextInputControl::_focus_exited() {
	_emit("topEndEditing");
	_emit("topBlur");
}
void RNTextInputControl::_editor_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed()) {
		return;
	}
	Dictionary event;
	event["key"] = key->get_keycode() == Key::BACKSPACE ? "Backspace" : key->get_keycode() == Key::ENTER || key->get_keycode() == Key::KP_ENTER ? "Enter"
			: key->get_unicode()																												? String::chr(key->get_unicode())
																																				: key->as_text_keycode();
	_emit("topKeyPress", event);
	if (Object::cast_to<TextEdit>(editor) && (key->get_keycode() == Key::ENTER || key->get_keycode() == Key::KP_ENTER) && _submit_behavior() != "newline" && !is_composing()) {
		_submitted(get_text());
		editor->accept_event();
	}
}
void RNTextInputControl::focus() {
	editor->grab_focus();
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		line->edit();
	}
}
void RNTextInputControl::blur() {
	editor->release_focus();
}
void RNTextInputControl::_font_changed() {
	if (layout_callback) {
		layout_callback();
	}
}
void RNTextInputControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE && auto_focus_pending) {
		callable_mp(this, &RNTextInputControl::_auto_focus).call_deferred();
	}
	if (p_what == NOTIFICATION_RESIZED && editor) {
		editor->set_size(get_size());
	}
	if (p_what == NOTIFICATION_PROCESS) {
		if (!is_composing() && !deferred_replacement.is_empty()) {
			const Dictionary pending = deferred_replacement.duplicate();
			deferred_replacement.clear();
			reconcile(pending["ack"], pending["text"], pending["start"], pending["end"]);
		}
		_selection_changed();
	}
}
