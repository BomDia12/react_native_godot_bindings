#include "rn_text_control.h"

#include "core/object/class_db.h"
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
			if (selection_to > selection_from) {
				const Color color = has_selection_color ? selection_color : get_theme_color(SNAME("selection_color"), SNAME("RichTextLabel"));
				for (const Rect2 &rect : layout.get_range_rects(selection_from, selection_to)) {
					draw_rect(rect, color);
				}
			}
			layout.draw(get_canvas_item(), Point2());
		} break;
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			const RID element = get_accessibility_element();
			ERR_FAIL_COND(element.is_null());
			AccessibilityServer::get_singleton()->update_set_role(element, AccessibilityServerEnums::AccessibilityRole::ROLE_STATIC_TEXT);
			AccessibilityServer::get_singleton()->update_set_value(element, text);
			AccessibilityServer::get_singleton()->update_set_text_align(element, accessibility_alignment);
		} break;
	}
}

void RNTextControl::gui_input(const Ref<InputEvent> &p_event) {
	if (!is_selectable()) {
		return;
	}
	Ref<InputEventMouseButton> button = p_event;
	if (button.is_valid() && button->get_button_index() == MouseButton::LEFT) {
		selecting = button->is_pressed();
		if (selecting) {
			selection_anchor = layout.hit_test(button->get_position());
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
	if (p_event->is_action_pressed(SNAME("ui_copy"), false, true) && selection_to > selection_from) {
		DisplayServer::get_singleton()->clipboard_set(text.substr(selection_from, selection_to - selection_from));
		accept_event();
	}
}

void RNTextControl::_shape() {
	if (shape_function) {
		shape_function(get_size().x, layout);
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

void RNTextControl::set_selection_color(const Color &p_color) {
	selection_color = p_color;
	has_selection_color = true;
	queue_redraw();
}
