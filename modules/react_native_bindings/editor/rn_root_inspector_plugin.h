#pragma once

#ifdef TOOLS_ENABLED

#include "editor/inspector/editor_inspector.h"
#include "scene/gui/dialogs.h"

class LineEdit;
class OptionButton;
class VBoxContainer;

// Creates a component contract from a name, typed properties and events.
// Defaults and named record types are edited on the saved resource.
class RNContractDialog : public ConfirmationDialog {
	GDCLASS(RNContractDialog, ConfirmationDialog);

	LineEdit *component = nullptr;
	VBoxContainer *properties = nullptr;
	VBoxContainer *events = nullptr;
	Label *error = nullptr;

	void _add_property();
	void _add_event();
	void _create();

protected:
	static void _bind_methods();

public:
	RNContractDialog();
	void open();
};

// Picks the root's application key among the project's contract components
// and the keys the editor's runtime registers.
class RNComponentPicker : public EditorProperty {
	GDCLASS(RNComponentPicker, EditorProperty);

	OptionButton *options = nullptr;
	RNContractDialog *dialog = nullptr;
	PackedStringArray runtime_keys;

	void _list();
	void _selected(int p_index);
	void _keys_received(const Variant &p_keys);
	void _created(const String &p_component);

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	RNComponentPicker();
	void update_property() override;
};

class RNRootInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(RNRootInspectorPlugin, EditorInspectorPlugin);

protected:
	static void _bind_methods() {}

public:
	bool can_handle(Object *p_object) override;
	void parse_begin(Object *p_object) override;
	bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

#endif // TOOLS_ENABLED
