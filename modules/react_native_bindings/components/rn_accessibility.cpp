#include "rn_accessibility.h"

#include "core/templates/hash_map.h"
#include "servers/display/accessibility_server.h"

using Role = AccessibilityServerEnums::AccessibilityRole;

AccessibilityServerEnums::AccessibilityRole rn_accessibility_role(const String &p_name, String &r_description) {
	static const HashMap<String, Role> roles = {
		{ "button", Role::ROLE_BUTTON },
		{ "togglebutton", Role::ROLE_BUTTON },
		{ "link", Role::ROLE_LINK },
		{ "checkbox", Role::ROLE_CHECK_BOX },
		{ "radio", Role::ROLE_RADIO_BUTTON },
		{ "switch", Role::ROLE_CHECK_BUTTON },
		{ "adjustable", Role::ROLE_SLIDER },
		{ "slider", Role::ROLE_SLIDER },
		{ "spinbutton", Role::ROLE_SPIN_BUTTON },
		{ "progressbar", Role::ROLE_PROGRESS_INDICATOR },
		{ "scrollbar", Role::ROLE_SCROLL_BAR },
		{ "image", Role::ROLE_IMAGE },
		{ "img", Role::ROLE_IMAGE },
		{ "text", Role::ROLE_STATIC_TEXT },
		{ "summary", Role::ROLE_STATIC_TEXT },
		{ "search", Role::ROLE_TEXT_FIELD },
		{ "list", Role::ROLE_LIST },
		{ "listitem", Role::ROLE_LIST_ITEM },
		{ "grid", Role::ROLE_TABLE },
		{ "table", Role::ROLE_TABLE },
		{ "row", Role::ROLE_ROW },
		{ "cell", Role::ROLE_CELL },
		{ "gridcell", Role::ROLE_CELL },
		{ "rowheader", Role::ROLE_ROW_HEADER },
		{ "columnheader", Role::ROLE_COLUMN_HEADER },
		{ "menu", Role::ROLE_MENU },
		{ "menubar", Role::ROLE_MENU_BAR },
		{ "menuitem", Role::ROLE_MENU_ITEM },
		{ "tab", Role::ROLE_TAB },
		{ "tablist", Role::ROLE_TAB_BAR },
		{ "tabbar", Role::ROLE_TAB_BAR },
		{ "tabpanel", Role::ROLE_TAB_PANEL },
		{ "dialog", Role::ROLE_DIALOG },
		{ "alertdialog", Role::ROLE_DIALOG },
		{ "toolbar", Role::ROLE_REGION },
		{ "group", Role::ROLE_REGION },
		{ "region", Role::ROLE_REGION },
		{ "tooltip", Role::ROLE_TOOLTIP },
		{ "tree", Role::ROLE_TREE },
		{ "treeitem", Role::ROLE_TREE_ITEM },
		{ "combobox", Role::ROLE_LIST_BOX },
		{ "option", Role::ROLE_LIST_BOX_OPTION },
	};
	r_description = String();
	if (const Role *role = roles.getptr(p_name)) {
		return *role;
	}
	r_description = p_name;
	return Role::ROLE_CONTAINER;
}

void rn_publish_accessibility(const RNAccessibility &p_accessibility, const RID &p_element) {
	AccessibilityServer *server = AccessibilityServer::get_singleton();
	if (p_accessibility.role) {
		server->update_set_role(p_element, *p_accessibility.role);
	}
	if (!p_accessibility.role_description.is_empty()) {
		server->update_set_role_description(p_element, p_accessibility.role_description);
	}
	if (p_accessibility.checked) {
		server->update_set_checked(p_element, *p_accessibility.checked);
	}
	// Godot's checked state is boolean.
	if (p_accessibility.mixed) {
		server->update_set_checked(p_element, false);
		server->update_set_state_description(p_element, "mixed");
	}
	if (p_accessibility.selected) {
		server->update_set_list_item_selected(p_element, *p_accessibility.selected);
	}
	if (p_accessibility.expanded) {
		server->update_set_list_item_expanded(p_element, *p_accessibility.expanded);
	}
	server->update_set_flag(p_element, AccessibilityServerEnums::FLAG_DISABLED, p_accessibility.disabled);
	server->update_set_flag(p_element, AccessibilityServerEnums::FLAG_BUSY, p_accessibility.busy);
	server->update_set_flag(p_element, AccessibilityServerEnums::FLAG_HIDDEN, p_accessibility.hidden);
	server->update_set_flag(p_element, AccessibilityServerEnums::FLAG_MODAL, p_accessibility.modal);
	if (p_accessibility.range) {
		server->update_set_num_range(p_element, p_accessibility.range->x, p_accessibility.range->y);
		server->update_set_num_value(p_element, p_accessibility.range->z);
	}
	if (!p_accessibility.value_text.is_empty()) {
		server->update_set_value(p_element, p_accessibility.value_text);
	}
}
