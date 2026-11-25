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
	// With a Godot StyleBox or theme type painting the box, the outline is all
	// that's left of the React Native paint.
	Ref<RNStyleBox> outline;
	Ref<StyleBox> skin;
	bool skinned = false;
	Dictionary drag_data;
	Ref<Texture2D> drag_preview;
	Array drop_types;
	bool drag_source = false;
	std::function<void(const Variant &, const Point2 &)> dropped;
	std::function<void(bool)> drag_ended;
	RNBoxShadows shadows;
	RNAccessibility accessibility;
	// A grouped view's children hang under this hidden element, as TabContainer
	// hides inactive tabs, because AccessKit rejects elements outside the tree.
	RID hidden_children;
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
	// Paints the box with p_skin or else p_theme_type's panel StyleBox, in place of
	// the React Native background and borders.
	void set_skin(const Ref<StyleBox> &p_skin, const StringName &p_theme_type);
	// Godot asks for these synchronously, so React declares them ahead of a drag.
	void set_drag(const Dictionary &p_data, const Ref<Texture2D> &p_preview, const Array &p_drop_types);
	void set_drag_callbacks(std::function<void(const Variant &, const Point2 &)> p_dropped, std::function<void(bool)> p_ended);
	void copy_drag_callbacks_to(RNViewControl *p_other) const { p_other->set_drag_callbacks(dropped, drag_ended); }
	Variant get_drag_data(const Point2 &p_point) override;
	bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	void drop_data(const Point2 &p_point, const Variant &p_data) override;
	void set_accessibility(const RNAccessibility &p_accessibility);
	bool is_accessibility_grouped() const { return accessibility.grouped; }
	// An accessible view is one element, as on iOS and Android: its
	// descendants are hidden from screen readers.
	bool accessibility_override_tree_hierarchy() const override { return accessibility.grouped; }
	// p_click runs for the default activation, p_action for named React Native actions.
	void set_accessibility_callbacks(std::function<void()> p_click, std::function<void(const String &)> p_action);
};
