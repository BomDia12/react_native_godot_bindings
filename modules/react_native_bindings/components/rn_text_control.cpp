#include "rn_text_control.h"

#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/display/accessibility_server.h"
#include "servers/display/display_server.h"

void RNTextControl::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_text"), &RNTextControl::get_text);
	ClassDB::bind_method(D_METHOD("get_font_size"), &RNTextControl::get_font_size);
	ClassDB::bind_method(D_METHOD("get_text_color"), &RNTextControl::get_text_color);
}

String RNTextControl::get_text() const {
	return text;
}

int RNTextControl::get_font_size() const {
	return layout.get_fragments().is_empty() ? 0 : layout.get_fragments()[0].font_size;
}

Color RNTextControl::get_text_color() const {
	return layout.get_fragments().is_empty() ? Color() : layout.get_fragments()[0].color;
}

RNTextControl::RNTextControl() {
	set_clip_contents(true);
	set_focus_mode(FOCUS_ACCESSIBILITY);
}

void RNTextControl::_notification(int p_what) {
	switch (p_what) {
		// Fabric sizes new views before inserting them, and Godot only reports resizes inside the tree.
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_RESIZED: {
			_shape();
		} break;
		case NOTIFICATION_DRAW: {
			if (box.is_valid()) {
				box->draw(get_canvas_item(), Rect2(Point2(), get_size()));
			}
			if (selection_to > selection_from) {
				const Color color = has_selection_color ? selection_color : get_theme_color(SNAME("selection_color"), SNAME("RichTextLabel"));
				for (const Rect2 &rect : layout.get_range_rects(selection_from, selection_to)) {
					draw_rect(rect, color);
				}
			}
			layout.draw(get_canvas_item(), Point2());
		} break;
		case NOTIFICATION_FOCUS_EXIT: {
			set_selection_range(-1, -1);
		} break;
		case NOTIFICATION_EXIT_TREE:
		case NOTIFICATION_ACCESSIBILITY_INVALIDATE: {
			accessibility_runs.clear();
		} break;
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			const RID element = get_accessibility_element();
			ERR_FAIL_COND(element.is_null());
			AccessibilityServer *server = AccessibilityServer::get_singleton();
			server->update_set_role(element, accessibility_role);
			if (!accessibility_role_description.is_empty()) {
				server->update_set_role_description(element, accessibility_role_description);
			}
			server->update_set_value(element, text);
			server->update_set_text_align(element, accessibility_alignment);
			const Vector<RNParagraphLayout::Line> &lines = layout.get_lines();
			if (accessibility_runs.is_empty()) {
				for (int i = 0; i < lines.size(); ++i) {
					accessibility_runs.push_back(server->create_sub_text_edit_elements(element, layout.get_line_rid(i), lines[i].bounds.size.y, -1, i == lines.size() - 1));
				}
			}
			for (int i = 0; i < accessibility_runs.size(); ++i) {
				server->update_set_transform(accessibility_runs[i], Transform2D(0, lines[i].bounds.position));
			}
			if (selection_to > selection_from && !accessibility_runs.is_empty()) {
				auto locate = [&lines](int p_index, int &r_line) {
					r_line = 0;
					while (r_line + 1 < lines.size() && p_index >= lines[r_line].range.y) {
						++r_line;
					}
					return p_index - lines[r_line].range.x;
				};
				int from_line;
				int to_line;
				const int from = locate(selection_from, from_line);
				const int to = locate(selection_to, to_line);
				server->update_set_text_selection(element, accessibility_runs[from_line], from, accessibility_runs[to_line], to);
			}
		} break;
	}
}

void RNTextControl::gui_input(const Ref<InputEvent> &p_event) {
	if (!is_selectable()) {
		return;
	}
	Ref<InputEventMouseButton> button = p_event;
	if (button.is_valid() && button->get_button_index() == MouseButton::LEFT) {
		selecting = button->is_pressed() && !button->is_double_click();
		if (!button->is_pressed()) {
			return;
		}
		const int position = layout.hit_test(button->get_position());
		const uint64_t now = OS::get_singleton()->get_ticks_msec();
		if (button->is_double_click()) {
			last_double_click_msec = now;
			_select_word(position);
		} else if (now - last_double_click_msec < 400) {
			set_selection_range(0, text.length());
		} else if (button->is_shift_pressed() && selection_anchor >= 0) {
			set_selection_range(MIN(selection_anchor, position), MAX(selection_anchor, position));
		} else {
			selection_anchor = position;
			set_selection_range(-1, -1);
		}
		return;
	}
	Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid() && selecting) {
		const int position = layout.hit_test(motion->get_position());
		set_selection_range(MIN(selection_anchor, position), MAX(selection_anchor, position));
		return;
	}
	if (p_event->is_action_pressed(SNAME("ui_text_select_all"), false, true)) {
		set_selection_range(0, text.length());
		accept_event();
		return;
	}
	if (p_event->is_action_pressed(SNAME("ui_copy"), false, true) && selection_to > selection_from) {
		DisplayServer::get_singleton()->clipboard_set(text.substr(selection_from, selection_to - selection_from));
		accept_event();
	}
}

void RNTextControl::_select_word(int p_character) {
	const PackedInt32Array words = TS->string_get_word_breaks(text);
	for (int i = 0; i + 1 < words.size(); i += 2) {
		if (p_character >= words[i] && p_character < words[i + 1]) {
			selection_anchor = words[i];
			set_selection_range(words[i], words[i + 1]);
			return;
		}
	}
}

void RNTextControl::_free_accessibility_runs() {
	for (const RID &run : accessibility_runs) {
		AccessibilityServer::get_singleton()->free_element(run);
	}
	accessibility_runs.clear();
}

void RNTextControl::set_accessibility_role(AccessibilityServerEnums::AccessibilityRole p_role, const String &p_description) {
	accessibility_role = p_role;
	accessibility_role_description = p_description;
	queue_accessibility_update();
}

void RNTextControl::_shape() {
	_free_accessibility_runs();
	if (shape_function) {
		shape_function(get_size(), layout);
	}
	queue_redraw();
	queue_accessibility_update();
}

bool RNTextControl::is_selectable() const {
	return selectable;
}

void RNTextControl::set_selection_range(int p_from, int p_to) {
	const int length = text.length();
	const bool valid = is_selectable() && p_from >= 0 && p_to > p_from;
	const int from = valid ? MIN(p_from, length) : -1;
	const int to = valid ? MIN(p_to, length) : -1;
	if (from == selection_from && to == selection_to) {
		return;
	}
	selection_from = from;
	selection_to = to;
	queue_redraw();
	queue_accessibility_update();
}

void RNTextControl::set_content(const String &p_text, RNTextShapeFunction p_shape_function, bool p_selectable) {
	if (p_text != text) {
		set_selection_range(-1, -1);
	}
	text = p_text;
	shape_function = std::move(p_shape_function);
	selectable = p_selectable;
	set_focus_mode(selectable ? FOCUS_ALL : FOCUS_ACCESSIBILITY);
	_shape();
}

void RNTextControl::set_box(const Ref<StyleBox> &p_box) {
	box = p_box;
	queue_redraw();
}

void RNTextControl::set_selection_color(const Color &p_color) {
	selection_color = p_color;
	has_selection_color = true;
	queue_redraw();
}
