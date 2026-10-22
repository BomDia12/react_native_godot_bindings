#pragma once

#include "servers/display/accessibility_server.h"
#include "servers/display/display_server.h"

#include <optional>

// Records what scene nodes publish, the way AccessKit would receive it. Like
// AccessKit, an element keeps its role, name and sub-elements between updates
// and rebuilds everything else from the update_* calls of each pass.
class RNTestAccessibilityServer : public AccessibilityServer {
public:
	using Role = AccessibilityServerEnums::AccessibilityRole;
	using Action = AccessibilityServerEnums::AccessibilityAction;
	using Flag = AccessibilityServerEnums::AccessibilityFlags;

	struct Element {
		Role role = AccessibilityServerEnums::ROLE_UNKNOWN;
		String name, description, value, role_description, state_description;
		AccessibilityServerEnums::AccessibilityLiveMode live = AccessibilityServerEnums::LIVE_OFF;
		HashSet<int> flags;
		std::optional<bool> checked, selected, expanded;
		Vector2 num_range;
		double num_value = 0;
		HashMap<int, Callable> actions;
		HashMap<int, String> custom_actions;
		Vector<RID> children;
		Vector<RID> sub_elements;
		RID parent;
	};

private:
	AccessibilityServer *previous = nullptr;
	HashMap<RID, Element> elements;
	HashMap<DisplayServerEnums::WindowID, RID> window_roots;
	HashSet<RID> updated;
	// Nodes cache their element RIDs across servers, so ids stay unique per process.
	static inline uint64_t next_id = 1;

	RID _add(Role p_role, const RID &p_parent = RID()) {
		const RID id = RID::from_uint64(next_id++);
		Element element;
		element.role = p_role;
		element.parent = p_parent;
		elements.insert(id, element);
		return id;
	}
	Element *_get(const RID &p_id) { return elements.getptr(p_id); }
	// The element as rebuilt by this update pass.
	Element *_update(const RID &p_id) {
		Element *found = _get(p_id);
		if (found && !updated.has(p_id)) {
			updated.insert(p_id);
			Element rebuilt;
			rebuilt.role = found->role;
			rebuilt.name = found->name;
			rebuilt.sub_elements = found->sub_elements;
			rebuilt.parent = found->parent;
			*found = rebuilt;
		}
		return found;
	}
	void _free(const RID &p_id) {
		if (const Element *found = _get(p_id)) {
			for (const RID &sub_element : Vector<RID>(found->sub_elements)) {
				_free(sub_element);
			}
		}
		elements.erase(p_id);
	}

public:
	// The base constructor replaces the singleton, so the previous server is captured first.
	explicit RNTestAccessibilityServer(AccessibilityServer *p_previous = get_singleton()) :
			previous(p_previous) {}
	~RNTestAccessibilityServer() override {
		singleton = previous;
	}

	const Element *element(const RID &p_id) const { return elements.getptr(p_id); }
	bool has_flag(const RID &p_id, Flag p_flag) const {
		const Element *found = element(p_id);
		return found && found->flags.has(p_flag);
	}
	bool perform(const RID &p_id, Action p_action, const Variant &p_data = Variant()) {
		const Element *found = element(p_id);
		if (!found || !found->actions.has(p_action)) {
			return false;
		}
		found->actions[p_action].call(p_data);
		return true;
	}

	bool is_supported() const override { return true; }

	bool window_create(DisplayServerEnums::WindowID p_window_id, void *) override {
		window_roots[p_window_id] = _add(AccessibilityServerEnums::ROLE_WINDOW);
		return true;
	}
	void window_destroy(DisplayServerEnums::WindowID p_window_id) override { window_roots.erase(p_window_id); }

	RID create_element(DisplayServerEnums::WindowID, Role p_role) override { return _add(p_role); }
	RID create_sub_element(const RID &p_parent_rid, Role p_role, int) override {
		const RID id = _add(p_role, p_parent_rid);
		if (Element *parent = _get(p_parent_rid)) {
			parent->sub_elements.push_back(id);
		}
		return id;
	}
	RID create_sub_text_edit_elements(const RID &p_parent_rid, const RID &, float, int p_insert_pos, bool) override {
		return create_sub_element(p_parent_rid, AccessibilityServerEnums::ROLE_TEXT_RUN, p_insert_pos);
	}
	bool has_element(const RID &p_id) const override { return elements.has(p_id); }
	void free_element(const RID &p_id) override {
		const Element *found = _get(p_id);
		if (Element *parent = found ? _get(found->parent) : nullptr) {
			parent->children.erase(p_id);
			parent->sub_elements.erase(p_id);
		}
		_free(p_id);
	}

	void element_set_meta(const RID &, const Variant &) override {}
	Variant element_get_meta(const RID &) const override { return Variant(); }

	void update_if_active(const Callable &p_callable) override {
		updated.clear();
		p_callable.call(DisplayServerEnums::MAIN_WINDOW_ID);
	}

	RID get_window_root(DisplayServerEnums::WindowID p_window_id) const override {
		const RID *root = window_roots.getptr(p_window_id);
		if (root) {
			return *root;
		}
		RNTestAccessibilityServer *self = const_cast<RNTestAccessibilityServer *>(this);
		return self->window_roots[p_window_id] = self->_add(AccessibilityServerEnums::ROLE_WINDOW);
	}
	void update_set_focus(const RID &) override {}

	void set_window_rect(DisplayServerEnums::WindowID, const Rect2 &, const Rect2 &) override {}
	void set_window_focused(DisplayServerEnums::WindowID, bool) override {}
	void set_window_callbacks(DisplayServerEnums::WindowID, const Callable &, const Callable &) override {}
	void window_activation_completed(DisplayServerEnums::WindowID) override {}
	void window_deactivation_completed(DisplayServerEnums::WindowID) override {}

	void update_set_role(const RID &p_id, Role p_role) override {
		if (Element *found = _update(p_id)) {
			found->role = p_role;
		}
	}
	void update_set_name(const RID &p_id, const String &p_name) override {
		if (Element *found = _update(p_id)) {
			found->name = p_name;
		}
	}
	void update_set_description(const RID &p_id, const String &p_description) override {
		if (Element *found = _update(p_id)) {
			found->description = p_description;
		}
	}
	void update_set_value(const RID &p_id, const String &p_value) override {
		if (Element *found = _update(p_id)) {
			found->value = p_value;
		}
	}
	void update_set_role_description(const RID &p_id, const String &p_description) override {
		if (Element *found = _update(p_id)) {
			found->role_description = p_description;
		}
	}
	void update_set_state_description(const RID &p_id, const String &p_description) override {
		if (Element *found = _update(p_id)) {
			found->state_description = p_description;
		}
	}
	void update_set_live(const RID &p_id, AccessibilityServerEnums::AccessibilityLiveMode p_live) override {
		if (Element *found = _update(p_id)) {
			found->live = p_live;
		}
	}
	void update_set_flag(const RID &p_id, Flag p_flag, bool p_value) override {
		if (Element *found = _update(p_id)) {
			if (p_value) {
				found->flags.insert(p_flag);
			} else {
				found->flags.erase(p_flag);
			}
		}
	}
	void update_set_checked(const RID &p_id, bool p_checked) override {
		if (Element *found = _update(p_id)) {
			found->checked = p_checked;
		}
	}
	void update_set_list_item_selected(const RID &p_id, bool p_selected) override {
		if (Element *found = _update(p_id)) {
			found->selected = p_selected;
		}
	}
	void update_set_list_item_expanded(const RID &p_id, bool p_expanded) override {
		if (Element *found = _update(p_id)) {
			found->expanded = p_expanded;
		}
	}
	void update_set_num_range(const RID &p_id, double p_min, double p_max) override {
		if (Element *found = _update(p_id)) {
			found->num_range = Vector2(p_min, p_max);
		}
	}
	void update_set_num_value(const RID &p_id, double p_value) override {
		if (Element *found = _update(p_id)) {
			found->num_value = p_value;
		}
	}
	void update_add_action(const RID &p_id, Action p_action, const Callable &p_callable) override {
		if (Element *found = _update(p_id)) {
			found->actions[p_action] = p_callable;
		}
	}
	void update_add_custom_action(const RID &p_id, int p_action_id, const String &p_action_description) override {
		if (Element *found = _update(p_id)) {
			found->custom_actions[p_action_id] = p_action_description;
		}
	}
	void update_add_child(const RID &p_id, const RID &p_child_id) override {
		Element *found = _update(p_id);
		Element *child = _get(p_child_id);
		if (found && child) {
			found->children.push_back(p_child_id);
			child->parent = p_id;
		}
	}

	void update_set_braille_label(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_braille_role_description(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_extra_info(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_tooltip(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_bounds(const RID &p_id, const Rect2 &) override { _update(p_id); }
	void update_set_transform(const RID &p_id, const Transform2D &) override { _update(p_id); }
	void update_add_related_controls(const RID &p_id, const RID &) override { _update(p_id); }
	void update_add_related_details(const RID &p_id, const RID &) override { _update(p_id); }
	void update_add_related_described_by(const RID &p_id, const RID &) override { _update(p_id); }
	void update_add_related_flow_to(const RID &p_id, const RID &) override { _update(p_id); }
	void update_add_related_labeled_by(const RID &p_id, const RID &) override { _update(p_id); }
	void update_add_related_radio_group(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_active_descendant(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_next_on_line(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_previous_on_line(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_member_of(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_in_page_link_target(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_error_message(const RID &p_id, const RID &) override { _update(p_id); }
	void update_set_table_row_count(const RID &p_id, int) override { _update(p_id); }
	void update_set_table_column_count(const RID &p_id, int) override { _update(p_id); }
	void update_set_table_row_index(const RID &p_id, int) override { _update(p_id); }
	void update_set_table_column_index(const RID &p_id, int) override { _update(p_id); }
	void update_set_table_cell_position(const RID &p_id, int, int) override { _update(p_id); }
	void update_set_table_cell_span(const RID &p_id, int, int) override { _update(p_id); }
	void update_set_list_item_count(const RID &p_id, int) override { _update(p_id); }
	void update_set_list_item_index(const RID &p_id, int) override { _update(p_id); }
	void update_set_list_item_level(const RID &p_id, int) override { _update(p_id); }
	void update_set_popup_type(const RID &p_id, AccessibilityServerEnums::AccessibilityPopupType) override { _update(p_id); }
	void update_set_num_step(const RID &p_id, double) override { _update(p_id); }
	void update_set_num_jump(const RID &p_id, double) override { _update(p_id); }
	void update_set_scroll_x(const RID &p_id, double) override { _update(p_id); }
	void update_set_scroll_x_range(const RID &p_id, double, double) override { _update(p_id); }
	void update_set_scroll_y(const RID &p_id, double) override { _update(p_id); }
	void update_set_scroll_y_range(const RID &p_id, double, double) override { _update(p_id); }
	void update_set_text_decorations(const RID &p_id, bool, bool, bool, const Color &) override { _update(p_id); }
	void update_set_text_align(const RID &p_id, HorizontalAlignment) override { _update(p_id); }
	void update_set_text_selection(const RID &p_id, const RID &, int, const RID &, int) override { _update(p_id); }
	void update_set_classname(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_placeholder(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_language(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_text_orientation(const RID &p_id, bool) override { _update(p_id); }
	void update_set_list_orientation(const RID &p_id, bool) override { _update(p_id); }
	void update_set_shortcut(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_url(const RID &p_id, const String &) override { _update(p_id); }
	void update_set_color_value(const RID &p_id, const Color &) override { _update(p_id); }
	void update_set_background_color(const RID &p_id, const Color &) override { _update(p_id); }
	void update_set_foreground_color(const RID &p_id, const Color &) override { _update(p_id); }
};
