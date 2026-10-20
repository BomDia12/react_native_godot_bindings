#include "rn_view_control.h"

#include "rn_text_control.h"

#include "core/object/callable_mp.h"
#include "servers/display/accessibility_server.h"

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

void RNViewControl::set_accessibility(const RNAccessibility &p_accessibility) {
	accessibility = p_accessibility;
	queue_accessibility_update();
}

void RNViewControl::set_accessibility_callbacks(std::function<void()> p_click, std::function<void(const String &)> p_action) {
	accessibility_click = std::move(p_click);
	accessibility_action = std::move(p_action);
}

// React Native activates with an explicit activate action when one is listed,
// and otherwise with the same click a keyboard press sends.
void RNViewControl::_accessibility_activate(const Variant &) {
	if (accessibility.activate && accessibility_action) {
		accessibility_action("activate");
	} else if (accessibility_click) {
		accessibility_click();
	}
}

void RNViewControl::_accessibility_named(const Variant &, const String &p_name) {
	if (accessibility_action) {
		accessibility_action(p_name);
	}
}

void RNViewControl::_accessibility_custom(const Variant &p_data) {
	const int index = p_data;
	if (accessibility_action && index >= 0 && index < accessibility.custom_names.size()) {
		accessibility_action(accessibility.custom_names[index]);
	}
}

// The text of the React Native views inside this one, in order, as iOS names
// an accessible group.
String RNViewControl::_descendant_text() const {
	PackedStringArray parts;
	Vector<const Node *> pending = { this };
	while (!pending.is_empty()) {
		const Node *node = pending[pending.size() - 1];
		pending.resize(pending.size() - 1);
		if (const RNTextControl *text = Object::cast_to<RNTextControl>(node)) {
			parts.push_back(text->get_text());
		}
		for (int i = node->get_child_count(true) - 1; i >= 0; --i) {
			pending.push_back(node->get_child(i, true));
		}
	}
	return String(" ").join(parts);
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
		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			const RID element = get_accessibility_element();
			ERR_FAIL_COND(element.is_null());
			rn_publish_accessibility(accessibility, element);
			AccessibilityServer *server = AccessibilityServer::get_singleton();
			if (accessibility.grouped && get_accessibility_name().is_empty()) {
				server->update_set_name(element, _descendant_text());
			}
			if (accessibility.clickable) {
				server->update_add_action(element, AccessibilityServerEnums::ACTION_CLICK, callable_mp(this, &RNViewControl::_accessibility_activate));
			}
			for (const RNNamedAccessibilityAction &known : RN_NAMED_ACCESSIBILITY_ACTIONS) {
				// Activation goes through the click action above.
				if (known.action != AccessibilityServerEnums::ACTION_CLICK && accessibility.*known.listed) {
					server->update_add_action(element, known.action, callable_mp(this, &RNViewControl::_accessibility_named).bind(String(known.name)));
				}
			}
			for (int i = 0; i < accessibility.custom_names.size(); ++i) {
				server->update_add_custom_action(element, i, accessibility.custom_labels[i]);
			}
			if (!accessibility.custom_names.is_empty()) {
				server->update_add_action(element, AccessibilityServerEnums::ACTION_CUSTOM, callable_mp(this, &RNViewControl::_accessibility_custom));
			}
		} break;
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
