#pragma once

#include "rn_accessibility.h"
#include "rn_box_shadows.h"
#include "rn_style_box.h"

#include "scene/gui/panel.h"
#include "scene/gui/popup_menu.h"

#include <functional>
#include <memory>

class RNViewControl : public Panel {
	GDCLASS(RNViewControl, Panel);

	PopupMenu *menu = nullptr;
	Array published_entries;
	std::function<void(const String &)> menu_action;
	std::function<void()> menu_opening;
	std::function<void(bool)> focus_changed;
	Ref<RNStyleBox> style;
	RNBoxShadows shadows;
	RNAccessibility accessibility;
	std::function<void()> accessibility_click;
	std::function<void(const String &)> accessibility_action;
	void _menu_selected(int p_id);
	void _accessibility_activate(const Variant &p_data);
	void _accessibility_named(const Variant &p_data, const String &p_name);
	void _accessibility_custom(const Variant &p_data);
	String _descendant_text() const;
	void _update_shadow_clip();

protected:
	static void _bind_methods() {}
	void gui_input(const Ref<InputEvent> &p_event) override;
	void _notification(int p_what);

public:
	RNViewControl();
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	void set_menu_entries(const Array &p_entries);
	void set_focus_callback(std::function<void(bool)> p_callback) { focus_changed = std::move(p_callback); }
	void set_menu_callbacks(std::function<void(const String &)> p_action, std::function<void()> p_opening);
	// Paints the view box, or nothing when p_visible is false.
	void set_paint(const RNStyleBox::Paint &p_paint, bool p_visible = true);
	Ref<RNStyleBox> get_style() const { return style; }
	void set_accessibility(const RNAccessibility &p_accessibility);
	bool is_accessibility_grouped() const { return accessibility.grouped; }
	// An accessible view is one element, as on iOS and Android: its
	// descendants don't join the accessibility tree.
	bool accessibility_override_tree_hierarchy() const override { return accessibility.grouped; }
	// p_click runs for the default activation, p_action for named React Native actions.
	void set_accessibility_callbacks(std::function<void()> p_click, std::function<void(const String &)> p_action);
};
