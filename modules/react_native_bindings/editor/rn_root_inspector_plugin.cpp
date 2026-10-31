#include "rn_root_inspector_plugin.h"

#ifdef TOOLS_ENABLED

#include "../host/react_native_host.h"
#include "../interop/rn_component_contract.h"
#include "../root_view/react_native_root_view.h"
#include "rn_contract_codegen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/settings/project_settings_editor.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"

namespace {

enum PickerItem {
	CREATE_CONTRACT = -2,
	EDIT_CONTRACT = -3,
};

const char *TYPES[] = { "boolean", "integer", "number", "string", "Color", "Vector2", "Vector3" };

Variant default_for(const String &p_type) {
	const HashMap<String, Variant> defaults = { { "boolean", false }, { "integer", 0 }, { "number", 0.0 }, { "string", "" }, { "Color", Color(1, 1, 1) }, { "Vector2", Vector2() }, { "Vector3", Vector3() } };
	return defaults.has(p_type) ? defaults[p_type] : Variant();
}

PackedStringArray contract_components() {
	PackedStringArray components;
	const String directory = rn_contracts_directory();
	for (const String &file : DirAccess::exists(directory) ? DirAccess::get_files_at(directory) : PackedStringArray()) {
		if (file.get_extension() == "tres") {
			components.push_back(file.get_basename());
		}
	}
	components.sort();
	return components;
}

HBoxContainer *removable_row(Control *p_first) {
	HBoxContainer *row = memnew(HBoxContainer);
	row->add_child(p_first);
	return row;
}

void add_remove_button(HBoxContainer *p_row) {
	Button *remove = memnew(Button);
	remove->set_text("Remove");
	remove->connect(SceneStringName(pressed), callable_mp(static_cast<Node *>(p_row), &Node::queue_free));
	p_row->add_child(remove);
}

} // namespace

void RNContractDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("created", PropertyInfo(Variant::STRING, "component")));
}

RNContractDialog::RNContractDialog() {
	set_title("New component contract");
	set_ok_button_text("Create");
	VBoxContainer *content = memnew(VBoxContainer);
	content->set_custom_minimum_size(Size2(520, 0));
	add_child(content);
	component = memnew(LineEdit);
	component->set_placeholder("ComponentName");
	content->add_child(component);
	properties = memnew(VBoxContainer);
	events = memnew(VBoxContainer);
	const std::pair<const char *, std::pair<VBoxContainer *, Callable>> sections[] = {
		{ "Properties", { properties, callable_mp(this, &RNContractDialog::_add_property) } },
		{ "Events (arguments as name: type, ...)", { events, callable_mp(this, &RNContractDialog::_add_event) } },
	};
	for (const auto &[heading, section] : sections) {
		Label *label = memnew(Label);
		label->set_text(heading);
		content->add_child(label);
		content->add_child(section.first);
		Button *add = memnew(Button);
		add->set_text("Add");
		add->connect(SceneStringName(pressed), section.second);
		content->add_child(add);
	}
	error = memnew(Label);
	error->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(error);
	get_ok_button()->connect(SceneStringName(pressed), callable_mp(this, &RNContractDialog::_create));
	set_hide_on_ok(false);
}

void RNContractDialog::open() {
	component->clear();
	error->set_text(String());
	for (VBoxContainer *section : { properties, events }) {
		for (int i = section->get_child_count() - 1; i >= 0; --i) {
			section->get_child(i)->queue_free();
		}
	}
	popup_centered();
	component->grab_focus();
}

void RNContractDialog::_add_property() {
	LineEdit *name = memnew(LineEdit);
	name->set_placeholder("snake_case_name");
	name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	HBoxContainer *row = removable_row(name);
	OptionButton *type = memnew(OptionButton);
	for (const char *type_name : TYPES) {
		type->add_item(type_name);
	}
	row->add_child(type);
	CheckBox *nullable = memnew(CheckBox);
	nullable->set_text("Nullable");
	row->add_child(nullable);
	add_remove_button(row);
	properties->add_child(row);
}

void RNContractDialog::_add_event() {
	LineEdit *name = memnew(LineEdit);
	name->set_placeholder("snake_case_name");
	HBoxContainer *row = removable_row(name);
	LineEdit *arguments = memnew(LineEdit);
	arguments->set_placeholder("enemy_id: string");
	arguments->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->add_child(arguments);
	add_remove_button(row);
	events->add_child(row);
}

void RNContractDialog::_create() {
	Ref<RNComponentContract> contract;
	contract.instantiate();
	contract->set_component(component->get_text().strip_edges());
	Array property_list;
	for (int i = 0; i < properties->get_child_count(); ++i) {
		Node *row = properties->get_child(i);
		if (row->is_queued_for_deletion()) {
			continue;
		}
		const String type = Object::cast_to<OptionButton>(row->get_child(1))->get_item_text(Object::cast_to<OptionButton>(row->get_child(1))->get_selected());
		const bool nullable = Object::cast_to<CheckBox>(row->get_child(2))->is_pressed();
		Dictionary value({ { "type", type } });
		if (nullable) {
			value["nullable"] = true;
		}
		property_list.push_back(Dictionary({ { "name", Object::cast_to<LineEdit>(row->get_child(0))->get_text().strip_edges() }, { "value", value }, { "default", nullable ? Variant() : default_for(type) } }));
	}
	contract->set_properties(property_list);
	Array event_list;
	for (int i = 0; i < events->get_child_count(); ++i) {
		Node *row = events->get_child(i);
		if (row->is_queued_for_deletion()) {
			continue;
		}
		Array arguments;
		for (const String &argument : Object::cast_to<LineEdit>(row->get_child(1))->get_text().split(",", false)) {
			arguments.push_back(Dictionary({ { "name", argument.get_slicec(':', 0).strip_edges() }, { "value", Dictionary({ { "type", argument.get_slicec(':', 1).strip_edges() } }) } }));
		}
		event_list.push_back(Dictionary({ { "name", Object::cast_to<LineEdit>(row->get_child(0))->get_text().strip_edges() }, { "arguments", arguments } }));
	}
	contract->set_events(event_list);

	RNCompiledContract compiled;
	RNError compile_error;
	const String path = rn_contracts_directory().path_join(contract->get_component() + ".tres");
	if (!contract->compile(compiled, compile_error)) {
		error->set_text(compile_error.describe());
		return;
	}
	if (FileAccess::exists(path)) {
		error->set_text(vformat("%s already exists.", path));
		return;
	}
	DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(rn_contracts_directory()));
	if (ResourceSaver::save(contract, path) != OK || !rn_generate_contracts(compile_error)) {
		error->set_text(compile_error.is_set() ? compile_error.describe() : vformat("Could not save %s.", path));
		return;
	}
	EditorFileSystem::get_singleton()->scan_changes();
	hide();
	emit_signal(SNAME("created"), contract->get_component());
}

RNComponentPicker::RNComponentPicker() {
	options = memnew(OptionButton);
	options->set_clip_text(true);
	options->connect(SceneStringName(item_selected), callable_mp(this, &RNComponentPicker::_selected));
	add_child(options);
	add_focusable(options);
	dialog = memnew(RNContractDialog);
	dialog->connect("created", callable_mp(this, &RNComponentPicker::_created));
	add_child(dialog);
}

// The editor's runtime knows which keys the bundle registers; reading them
// starts it, as previewing a root does, so a root that doesn't preview lists
// only its contracts.
void RNComponentPicker::_notification(int p_what) {
	const ReactNativeRootView *root = Object::cast_to<ReactNativeRootView>(get_edited_object());
	if (p_what == NOTIFICATION_ENTER_TREE && ReactNativeHost::get_singleton() && root && root->is_editor_preview()) {
		const Signal completed = ReactNativeHost::get_singleton()->evaluate("RN$AppRegistry.getAppKeys()");
		completed.get_object()->connect(completed.get_name(), callable_mp(this, &RNComponentPicker::_keys_received), CONNECT_ONE_SHOT);
	}
}

void RNComponentPicker::_keys_received(const Variant &p_keys) {
	runtime_keys.clear();
	for (const Variant &key : p_keys.get_type() == Variant::ARRAY ? Array(p_keys) : Array()) {
		if (String(key) != "LogBox") {
			runtime_keys.push_back(key);
		}
	}
	_list();
}

void RNComponentPicker::update_property() {
	_list();
}

void RNComponentPicker::_list() {
	const String current = get_edited_object()->get(get_edited_property());
	const PackedStringArray contracts = contract_components();
	options->clear();
	PackedStringArray keys = contracts;
	for (const String &key : runtime_keys) {
		if (!keys.has(key)) {
			keys.push_back(key);
		}
	}
	if (!keys.has(current)) {
		keys.push_back(current);
	}
	for (int i = 0; i < keys.size(); ++i) {
		options->add_item(keys[i]);
		options->set_item_metadata(i, keys[i]);
		if (!contracts.has(keys[i])) {
			options->set_item_tooltip(i, "This component has no contract.");
		}
		if (keys[i] == current) {
			options->select(i);
		}
	}
	options->add_separator();
	options->add_item("Create new…", CREATE_CONTRACT);
	if (contracts.has(current)) {
		options->add_item("Edit contract…", EDIT_CONTRACT);
	}
}

void RNComponentPicker::_selected(int p_index) {
	const int id = options->get_item_id(p_index);
	if (id == CREATE_CONTRACT) {
		_list();
		dialog->open();
	} else if (id == EDIT_CONTRACT) {
		_list();
		EditorInterface::get_singleton()->edit_resource(rn_load_contract(get_edited_object()->get(get_edited_property())));
	} else {
		emit_changed(get_edited_property(), options->get_item_metadata(p_index));
	}
}

void RNComponentPicker::_created(const String &p_component) {
	emit_changed(get_edited_property(), p_component);
}

bool RNRootInspectorPlugin::can_handle(Object *p_object) {
	return Object::cast_to<ReactNativeRootView>(p_object) != nullptr;
}

void RNRootInspectorPlugin::parse_begin(Object *) {
	HBoxContainer *summary = memnew(HBoxContainer);
	Label *runtime = memnew(Label);
	runtime->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	runtime->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	const bool development = GLOBAL_GET("react_native/dev_server/enabled");
	runtime->set_text(vformat("Bundle %s\n%s", GLOBAL_GET("react_native/bundle/path"), development ? vformat("Development: Metro on %s:%d", GLOBAL_GET("react_native/dev_server/host"), int(GLOBAL_GET("react_native/dev_server/port"))) : String("Release bundle; the developer server is off")));
	summary->add_child(runtime);
	Button *settings = memnew(Button);
	settings->set_text("Project Settings…");
	settings->connect(SceneStringName(pressed), callable_mp(EditorNode::get_singleton()->get_project_settings(), &ProjectSettingsEditor::popup_project_settings).bind(false));
	summary->add_child(settings);
	add_custom_control(summary);
}

bool RNRootInspectorPlugin::parse_property(Object *, const Variant::Type, const String &p_path, const PropertyHint, const String &, const BitField<PropertyUsageFlags>, const bool) {
	if (p_path != "application_key") {
		return false;
	}
	add_property_editor(p_path, memnew(RNComponentPicker));
	return true;
}

#endif // TOOLS_ENABLED
