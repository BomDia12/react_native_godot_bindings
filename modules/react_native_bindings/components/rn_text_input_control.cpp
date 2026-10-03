#include "rn_text_input_control.h"

#include "../fabric/rn_shadow_node.h"
#include "../fabric/rn_view_style.h"

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
Dictionary input_config() {
	Dictionary events;
	for (const char *name : { "Change", "SelectionChange", "SubmitEditing", "EndEditing", "Focus", "Blur", "KeyPress", "ContentSizeChange" }) {
		Dictionary registration;
		registration["registrationName"] = String("on") + name;
		events[String("top") + name] = registration;
	}
	Dictionary config;
	config["directEventTypes"] = events;
	return config;
}
class RNTextInputDescriptor : public RNHostDescriptor {
public:
	RNTextInputDescriptor() :
			RNHostDescriptor("GodotTextInput", RNHostTraits{ true, true, false, true, false, false, true, true, false, true, true }, input_config()) {}
	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		for (const char *key : { "text", "placeholder", "submitBehavior" }) {
			if (r_state.props.has(key) && r_state.props[key].get_type() != Variant::STRING) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "TextInput string prop is invalid", "textInput.prepare", key);
				return false;
			}
		}
		for (const char *key : { "mostRecentEventCount", "maxLength" }) {
			const Variant value = r_state.props.get(key, 0);
			const double number = value;
			if ((value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT) || !Math::is_finite(number) || number < 0 || number > 10000000 || Math::floor(number) != number) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "TextInput count/length must be a bounded integer", "textInput.prepare", key);
				return false;
			}
		}
		if (r_state.props.has("selection")) {
			const Variant value = r_state.props["selection"];
			if (value.get_type() != Variant::DICTIONARY) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "TextInput selection requires a record", "textInput.prepare");
				return false;
			}
			const Dictionary selection = value;
			for (const char *key : { "start", "end" }) {
				double offset = selection.get(key, selection.get("start", 0));
				if (!Math::is_finite(offset) || offset < 0 || offset > 10000000 || Math::floor(offset) != offset) {
					r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid TextInput selection", "textInput.prepare", key);
					return false;
				}
			}
		}
		if (bool(r_state.props.get("multiline", false)) && bool(r_state.props.get("secureTextEntry", false))) {
			r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Secure multiline entry is unavailable", "textInput.prepare");
			return false;
		}
		r_state.component_data = std::make_shared<RNTextInputData>();
		return true;
	}
	bool resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &r_error) const override {
		auto component = std::make_shared<RNTextInputData>();
		if (!rn_resolve_font(r_state.props, p_context, component->font, r_error)) {
			return false;
		}
		r_state.component_data = component;
		r_state.dependency_revision = component->font.revision;
		return true;
	}
	Control *create_host(const RNHostContext &) const override { return memnew(RNTextInputControl); }
	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		Object::cast_to<RNTextInputControl>(p_host)->apply(p_state);
		return true;
	}
	Variant capture_state(Control *p_host) const override { return Object::cast_to<RNTextInputControl>(p_host)->capture_state(); }
	void restore_state(Control *p_host, const Variant &p_state) const override { Object::cast_to<RNTextInputControl>(p_host)->restore_state(p_state); }
	void after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const override { Object::cast_to<RNTextInputControl>(p_host)->publish(p_state, p_context); }
	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &) const override {
		auto component = std::static_pointer_cast<const RNTextInputData>(p_state.component_data);
		return Size2(120, (component ? component->font.size : 16) * (bool(p_state.props.get("multiline", false)) ? 3 : 1) + 12);
	}
	Control *focus_control(Control *p_host) const override { return Object::cast_to<RNTextInputControl>(p_host)->get_editor(); }
	bool owns_native_activation(const RNPreparedHostState &) const override { return true; }
	bool owns_input_control(Control *p_host, Control *p_control) const override {
		auto *input = Object::cast_to<RNTextInputControl>(p_host);
		return p_control == input || p_control == input->get_editor() || input->get_editor()->is_ancestor_of(p_control);
	}
	bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const override {
		auto *input = Object::cast_to<RNTextInputControl>(p_host);
		const Array args = p_arguments;
		if (p_command == "focus") {
			input->focus();
			return true;
		}
		if (p_command == "blur") {
			input->blur();
			return true;
		}
		if (p_command == "setTextAndSelection" && args.size() == 4) {
			input->reconcile(args[0], args[1], args[2], args[3]);
			return true;
		}
		return RNHostDescriptor::dispatch_command(p_host, p_command, p_arguments, p_context, r_error);
	}
};
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
		auto *edit = memnew(TextEdit);
		edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
		edit->connect("text_changed", callable_mp(this, &RNTextInputControl::_native_changed));
		edit->connect("caret_changed", callable_mp(this, &RNTextInputControl::_selection_changed));
		result = edit;
	} else {
		auto *line = memnew(LineEdit);
		line->connect("text_changed", callable_mp(this, &RNTextInputControl::_line_changed));
		line->connect("text_submitted", callable_mp(this, &RNTextInputControl::_submitted));
		result = line;
	}
	result->set_external_layout_enabled(true);
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
void RNTextInputControl::_discard_notifications() {
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		line->discard_pending_edit_notifications();
	} else {
		Object::cast_to<TextEdit>(editor)->discard_pending_edit_notifications();
	}
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
	_discard_notifications();
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
	_discard_notifications();
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
void RNTextInputControl::_configure_editor(Control *p_editor, const RNPreparedHostState &p_state) {
	p_editor->set_mouse_filter(get_mouse_filter() == MOUSE_FILTER_IGNORE ? MOUSE_FILTER_IGNORE : MOUSE_FILTER_STOP);
	const auto component = std::static_pointer_cast<const RNTextInputData>(p_state.component_data);
	p_editor->add_theme_font_override("font", component->font.font);
	p_editor->add_theme_font_size_override("font_size", component->font.size);
	p_editor->add_theme_color_override("font_color", component->font.color);
	for (const char *key : { "selectionColor", "cursorColor", "placeholderTextColor" }) {
		Color color;
		const char *native = String(key) == "selectionColor" ? "selection_color" : String(key) == "cursorColor" ? "caret_color"
																												: "font_placeholder_color";
		if (RNViewStyle::color_of(p_state.props, key, color)) {
			p_editor->add_theme_color_override(native, color);
		} else {
			p_editor->remove_theme_color_override(native);
		}
	}
	const bool editable = bool(p_state.props.get("editable", true)) && !bool(p_state.props.get("readOnly", false));
	const String placeholder = p_state.props.get("placeholder", String());
	if (auto *line = Object::cast_to<LineEdit>(p_editor)) {
		line->set_editable(editable);
		line->set_placeholder(placeholder);
		line->set_secret(p_state.props.get("secureTextEntry", false));
		line->set_keep_editing_on_text_submit(String(p_state.props.get("submitBehavior", "blurAndSubmit")) == "submit");
	} else {
		auto *edit = Object::cast_to<TextEdit>(p_editor);
		edit->set_editable(editable);
		edit->set_placeholder(placeholder);
	}
	p_editor->set_size(get_size());
}
void RNTextInputControl::apply(const RNPreparedHostState &p_state) {
	changing = true;
	props = p_state.props.duplicate(true);
	const String pointer_events = String(props.get("pointerEvents", "auto")).to_lower();
	const bool targetable = p_state.branch_targetable && pointer_events != "none" && pointer_events != "box-none";
	set_mouse_filter(targetable ? MOUSE_FILTER_PASS : MOUSE_FILTER_IGNORE);
	editor->set_mouse_filter(targetable ? MOUSE_FILTER_STOP : MOUSE_FILTER_IGNORE);
	const bool multiline = props.get("multiline", false);
	if (multiline != (Object::cast_to<TextEdit>(editor) != nullptr)) {
		if (staged_editor) {
			memdelete(staged_editor);
		}
		staged_editor = _create_editor(multiline);
		add_child(staged_editor);
		staged_editor->hide();
		_configure_editor(staged_editor, p_state);
	} else {
		if (staged_editor) {
			memdelete(staged_editor);
			staged_editor = nullptr;
		}
		_configure_editor(editor, p_state);
		const uint64_t next_text = (p_state.declarative_prop_revisions.has("text") ? p_state.declarative_prop_revisions["text"] : 0);
		const uint64_t next_selection = (p_state.declarative_prop_revisions.has("selection") ? p_state.declarative_prop_revisions["selection"] : 0);
		const bool fresh_text = props.has("text") && (text_revision != next_text);
		const bool fresh_selection = props.has("selection") && (selection_revision != next_selection);
		const Dictionary selection = props.get("selection", Dictionary());
		if (fresh_text || fresh_selection) {
			reconcile(props.get("mostRecentEventCount", 0), fresh_text ? props["text"] : Variant(), fresh_selection ? int(selection.get("start", 0)) : -1, fresh_selection ? int(selection.get("end", selection.get("start", 0))) : -1);
		}
		text_revision = next_text;
		selection_revision = next_selection;
	}
	set_visible(String(props.get("display", "flex")) != "none");
	set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(props)));
	changing = false;
}
void RNTextInputControl::publish(const RNPreparedHostState &p_state, const RNHostContext &p_context) {
	published_context = p_context;
	if (pending_native_event) {
		pending_native_event = false;
		_emit("topChange");
	}
	if (staged_editor) {
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
		const Dictionary requested = props.get("selection", Dictionary());
		const uint64_t next_text = p_state.declarative_prop_revisions.has("text") ? p_state.declarative_prop_revisions["text"] : 0;
		const uint64_t next_selection = p_state.declarative_prop_revisions.has("selection") ? p_state.declarative_prop_revisions["selection"] : 0;
		const bool fresh_text = props.has("text") && (text_revision != next_text);
		const bool fresh_selection = props.has("selection") && (selection_revision != next_selection);
		reconcile(props.get("mostRecentEventCount", 0), fresh_text ? props["text"] : Variant(), fresh_selection ? int(requested.get("start", 0)) : -1, fresh_selection ? int(requested.get("end", requested.get("start", 0))) : -1);
		text_revision = next_text;
		selection_revision = next_selection;
		if (auto *edit = Object::cast_to<TextEdit>(editor)) {
			edit->clear_undo_history();
		}
		if (was_focused) {
			focus();
		}
	}
	const auto component = std::static_pointer_cast<const RNTextInputData>(p_state.component_data);
	if (subscribed_font != component->font.base) {
		if (subscribed_font.is_valid()) {
			subscribed_font->disconnect("changed", callable_mp(this, &RNTextInputControl::_font_changed));
		}
		subscribed_font = component->font.base;
		if (subscribed_font.is_valid()) {
			subscribed_font->connect("changed", callable_mp(this, &RNTextInputControl::_font_changed));
		}
	}
	if (!published) {
		published = true;
		if (bool(props.get("autoFocus", false)) && is_inside_tree() && is_visible_in_tree()) {
			focus();
		}
	}
}
Ref<RNTextInputState> RNTextInputControl::capture_state() const {
	Ref<RNTextInputState> state;
	state.instantiate();
	state->editor_id = editor->get_instance_id();
	state->wrapper_mouse_filter = get_mouse_filter();
	state->editor_mouse_filter = editor->get_mouse_filter();
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		state->line = line->capture_edit_state();
	} else {
		state->multiline = Object::cast_to<TextEdit>(editor)->capture_edit_state();
	}
	state->props = props.duplicate(true);
	state->deferred = deferred_replacement.duplicate(true);
	state->event_count = event_count;
	state->text_revision = text_revision;
	state->selection_revision = selection_revision;
	state->observed_text = observed_text;
	state->observed_selection = observed_selection;
	state->pending_native_event = pending_native_event;
	return state;
}
void RNTextInputControl::restore_state(const Ref<RNTextInputState> &p_state) {
	if (p_state.is_null() || p_state->editor_id != editor->get_instance_id()) {
		return;
	}
	changing = true;
	if (staged_editor) {
		memdelete(staged_editor);
		staged_editor = nullptr;
	}
	if (auto *line = Object::cast_to<LineEdit>(editor)) {
		line->restore_edit_state(p_state->line);
	} else {
		Object::cast_to<TextEdit>(editor)->restore_edit_state(p_state->multiline);
	}
	set_mouse_filter(p_state->wrapper_mouse_filter);
	editor->set_mouse_filter(p_state->editor_mouse_filter);
	props = p_state->props;
	deferred_replacement = p_state->deferred;
	event_count = p_state->event_count;
	text_revision = p_state->text_revision;
	selection_revision = p_state->selection_revision;
	observed_text = p_state->observed_text;
	observed_selection = p_state->observed_selection;
	pending_native_event = p_state->pending_native_event;
	changing = false;
}
void RNTextInputControl::_emit(const StringName &p_event, Dictionary p_payload) {
	if (!changing && published_context.event_sink && published_context.event_sink->emit) {
		p_payload["text"] = get_text();
		p_payload["eventCount"] = event_count;
		published_context.event_sink->emit(published_context.tag, p_event, p_payload, published_context.revision);
	}
}
void RNTextInputControl::_line_changed(const String &) {
	_native_changed();
}
void RNTextInputControl::_native_changed() {
	if (is_composing() || get_text() == observed_text) {
		return;
	}
	const int maximum = props.get("maxLength", 0);
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
		_discard_notifications();
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
	if (String(props.get("submitBehavior", "blurAndSubmit")) == "blurAndSubmit") {
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
	if (Object::cast_to<TextEdit>(editor) && (key->get_keycode() == Key::ENTER || key->get_keycode() == Key::KP_ENTER) && String(props.get("submitBehavior", "newline")) != "newline" && !is_composing()) {
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
	if (published_context.event_sink && published_context.event_sink->invalidate_layout) {
		published_context.event_sink->invalidate_layout();
	}
}
void RNTextInputControl::_notification(int p_what) {
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
std::shared_ptr<const RNHostDescriptor> rn_text_input_descriptor() {
	return std::make_shared<RNTextInputDescriptor>();
}
