#pragma once

#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/rid.h"
#include "core/templates/vector.h"
#include "servers/display/accessibility_server_enums.h"

#include <optional>

// What a React Native view adds to Godot's accessibility node: its role,
// states, value and actions. Name, description and live region use the
// Control's own accessibility properties.
struct RNAccessibility {
	std::optional<AccessibilityServerEnums::AccessibilityRole> role;
	String role_description;
	std::optional<bool> checked;
	std::optional<bool> selected;
	std::optional<bool> expanded;
	bool mixed = false;
	bool disabled = false;
	bool busy = false;
	bool hidden = false;
	bool modal = false;
	// Minimum, maximum and current value.
	std::optional<Vector3> range;
	String value_text;
	bool clickable = false;
	// An accessible view with no label is named by the text inside it.
	bool grouped = false;
	// accessibilityActions handled by the screen reader's own actions.
	bool activate = false;
	bool increment = false;
	bool decrement = false;
	bool expand = false;
	bool collapse = false;
	// The other accessibilityActions, published as custom actions by index.
	Vector<String> custom_names;
	Vector<String> custom_labels;
};

// The Godot role for a React Native role name. Roles without a Godot
// equivalent map to a container described by the React Native name.
AccessibilityServerEnums::AccessibilityRole rn_accessibility_role(const String &p_name, String &r_description);
// Publishes the states and value; the caller adds the actions.
void rn_publish_accessibility(const RNAccessibility &p_accessibility, const RID &p_element);
