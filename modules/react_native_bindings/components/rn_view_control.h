#pragma once

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
	void _menu_selected(int p_id);
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
};
