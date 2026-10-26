#include "rn_view_control.h"

#include "core/object/callable_mp.h"

RNViewControl::RNViewControl() {
	style.instantiate();
	add_theme_style_override(SNAME("panel"), style);
}

void RNViewControl::set_paint(const RNStyleBox::Paint &p_paint, bool p_visible) {
	style->set_paint(p_visible ? p_paint : RNStyleBox::Paint());
	shadows.update(get_canvas_item(), p_visible ? p_paint : RNStyleBox::Paint());
	set_notify_transform(!shadows.is_empty());
	_update_shadow_clip();
	queue_redraw();
}

void RNViewControl::_update_shadow_clip() {
	if (!shadows.is_empty() && is_inside_tree()) {
		shadows.update_clip(rn_clip_stack(this));
	}
}

void RNViewControl::set_menu_callbacks(std::function<void(const String &)> p_action, std::function<void()> p_opening) {
	menu_action = std::move(p_action);
	menu_opening = std::move(p_opening);
}

void RNViewControl::set_menu_entries(const Array &p_entries) {
	if (p_entries == published_entries) {
		return;
	}
	published_entries = p_entries.duplicate(true);
	if (published_entries.is_empty() && !menu) {
		return;
	}
	if (!menu) {
		menu = memnew(PopupMenu);
		add_child(menu, false, INTERNAL_MODE_BACK);
		menu->connect("id_pressed", callable_mp(this, &RNViewControl::_menu_selected));
	}
	menu->hide();
	menu->clear();
	for (int i = 0; i < published_entries.size(); ++i) {
		const Dictionary entry = published_entries[i];
		if (bool(entry.get("separator", false))) {
			menu->add_separator(entry.get("label", String()), i);
		} else {
			menu->add_item(entry["label"], i);
			menu->set_item_disabled(i, entry.get("disabled", false));
		}
	}
}

void RNViewControl::_menu_selected(int p_id) {
	if (p_id < 0 || p_id >= published_entries.size() || !menu_action) {
		return;
	}
	const Dictionary entry = published_entries[p_id];
	if (bool(entry.get("disabled", false)) || bool(entry.get("separator", false))) {
		return;
	}
	menu_action(entry["id"]);
}

void RNViewControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_TRANSFORM_CHANGED:
		case RN_NOTIFICATION_CLIP_CHANGED: {
			_update_shadow_clip();
		} break;
	}
	if ((p_what == NOTIFICATION_FOCUS_ENTER || p_what == NOTIFICATION_FOCUS_EXIT) && focus_changed) {
		focus_changed(p_what == NOTIFICATION_FOCUS_ENTER);
	}
}

void RNViewControl::gui_input(const Ref<InputEvent> &p_event) {
	Panel::gui_input(p_event);
	if (!menu || published_entries.is_empty()) {
		return;
	}
	Point2 position = get_screen_transform().xform(get_size() / 2);
	bool open = false;
	if (Ref<InputEventMouseButton> button = p_event; button.is_valid()) {
		open = button->is_pressed() && button->get_button_index() == MouseButton::RIGHT;
		position = get_screen_transform().xform(button->get_position());
	} else if (Ref<InputEventKey> key = p_event; key.is_valid()) {
		open = key->is_pressed() && !key->is_echo() && (key->get_keycode() == Key::MENU || (key->get_keycode() == Key::F10 && key->is_shift_pressed()));
	}
	if (open) {
		if (menu_opening) {
			menu_opening();
		}
		menu->popup(Rect2i(Point2i(position), Size2i()));
	}
}
