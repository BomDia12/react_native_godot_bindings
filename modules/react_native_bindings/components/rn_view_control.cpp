#include "rn_view_control.h"

#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"

#include "core/object/callable_mp.h"
#include "core/templates/hash_set.h"

namespace {

Dictionary native_view_config() {
	Dictionary action;
	action["registrationName"] = "onGodotContextMenuAction";
	Dictionary accessibility;
	accessibility["registrationName"] = "onAccessibilityAction";
	Dictionary direct;
	direct["topGodotContextMenuAction"] = action;
	direct["topAccessibilityAction"] = accessibility;
	Dictionary config;
	config["directEventTypes"] = direct;
	return config;
}

class RNViewDescriptor : public RNHostDescriptor {
public:
	RNViewDescriptor() : RNHostDescriptor("RCTView", RNHostTraits{ true, true, true, false, false, false, true, true, true, true, true }, native_view_config()) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		for (const char *key : { "backgroundColor", "borderColor" }) {
			Color ignored;
			if (r_state.props.has(key) && !RNViewStyle::color_of(r_state.props, key, ignored)) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid View color", "view.prepare", key);
				return false;
			}
		}
		if (r_state.props.has("godotContextMenu")) {
			if (r_state.props["godotContextMenu"].get_type() != Variant::ARRAY) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "godotContextMenu must contain entries", "view.prepare");
				return false;
			}
			const Array entries = r_state.props["godotContextMenu"];
			HashSet<String> ids;
			if (entries.size() > 256) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "Too many context menu entries", "view.prepare");
				return false;
			}
			for (const Variant &entry : entries) {
				if (entry.get_type() != Variant::DICTIONARY) {
					r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid context menu entry", "view.prepare");
					return false;
				}
				const Dictionary item = entry;
				if (bool(item.get("separator", false))) {
					continue;
				}
				const String id = item.get("id", String());
				if (id.is_empty() || ids.has(id) || item.get("label", Variant()).get_type() != Variant::STRING) {
					r_error = RNError::make(RNErrorCode::VALIDATION, "Menu entries require unique IDs and labels", "view.prepare");
					return false;
				}
				ids.insert(id);
			}
		}
		return true;
	}

	Control *create_host(const RNHostContext &) const override { return memnew(RNViewControl); }

	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &, RNError &) const override {
		Panel *panel = Object::cast_to<Panel>(p_host);
		panel->set_visible(String(p_state.props.get("display", "flex")) != "none");
		panel->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
		panel->add_theme_style_override("panel", RNViewStyle::build_stylebox(p_state.props, p_state.layout_rtl));
		panel->set_clip_contents(RNViewStyle::clips_contents(p_state.props));
		const String pointer_events = String(p_state.props.get("pointerEvents", "auto")).to_lower();
		const bool branch_enabled = p_state.branch_targetable && pointer_events != "none";
		const bool self_targetable = branch_enabled && pointer_events != "box-none";
		panel->set_mouse_filter(branch_enabled ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
		panel->set_focus_mode(self_targetable && bool(p_state.props.get("focusable", false)) ? Control::FOCUS_ALL : Control::FOCUS_NONE);
		Dictionary accessibility;
		for (const char *key : { "accessible", "accessibilityLabel", "accessibilityRole", "accessibilityHint", "accessibilityState", "accessibilityValue", "accessibilityActions", "accessibilityElementsHidden", "importantForAccessibility" }) {
			if (p_state.props.has(key)) {
				accessibility[key] = p_state.props[key];
			}
		}
		panel->set_meta("react_native_accessibility", accessibility);
		return true;
	}

	Variant capture_state(Control *p_host) const override {
		Panel *panel = Object::cast_to<Panel>(p_host);
		Dictionary state;
		state["visible"] = panel->is_visible();
		state["modulate"] = panel->get_modulate();
		state["style"] = panel->get_theme_stylebox("panel");
		state["clip"] = panel->is_clipping_contents();
		state["mouse"] = int(panel->get_mouse_filter());
		state["focus"] = int(panel->get_focus_mode());
		state["accessibility"] = panel->get_meta("react_native_accessibility", Dictionary());
		return state;
	}

	void restore_state(Control *p_host, const Variant &p_state) const override {
		Panel *panel = Object::cast_to<Panel>(p_host);
		const Dictionary state = p_state;
		panel->set_visible(state["visible"]);
		panel->set_modulate(state["modulate"]);
		panel->add_theme_style_override("panel", state["style"]);
		panel->set_clip_contents(state["clip"]);
		panel->set_mouse_filter(Control::MouseFilter(int(state["mouse"])));
		panel->set_focus_mode(Control::FocusMode(int(state["focus"])));
		panel->set_meta("react_native_accessibility", state["accessibility"]);
	}

	void attach_signals(Control *p_host, const RNHostContext &p_context) const override {
		if (p_context.owner) {
			p_host->connect("focus_entered", callable_mp(p_context.owner, &ReactNativeRootView::_on_focus_entered).bind(p_context.tag, p_host->get_instance_id()));
			p_host->connect("focus_exited", callable_mp(p_context.owner, &ReactNativeRootView::_on_focus_exited).bind(p_context.tag, p_host->get_instance_id()));
		}
	}

	void after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const override {
		Object::cast_to<RNViewControl>(p_host)->publish_menu(p_state, p_context);
	}

	bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const override {
		if (p_command == "focus") {
			p_host->grab_focus();
			return true;
		}
		if (p_command == "blur") {
			p_host->release_focus();
			return true;
		}
		return RNHostDescriptor::dispatch_command(p_host, p_command, p_arguments, p_context, r_error);
	}

	bool dispatch_accessibility_action(Control *p_host, const StringName &p_action, const Variant &, const RNHostContext &p_context, RNError &r_error) const override {
		const Dictionary metadata = p_host->get_meta("react_native_accessibility", Dictionary());
		const Array actions = metadata.get("accessibilityActions", Array());
		for (const Dictionary action : actions) {
			if (StringName(action.get("name", String())) == p_action && p_context.event_sink && p_context.event_sink->emit) {
				Dictionary payload;
				payload["actionName"] = String(p_action);
				p_context.event_sink->emit(p_context.tag, "topAccessibilityAction", payload, p_context.revision);
				return true;
			}
		}
		r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Accessibility action is not declared", "view.accessibility");
		return false;
	}
};

} // namespace

void RNViewControl::publish_menu(const RNPreparedHostState &p_state, const RNHostContext &p_context) {
	published_context = p_context;
	const Array entries = p_state.branch_targetable && String(p_state.props.get("pointerEvents", "auto")) != "none" && String(p_state.props.get("pointerEvents", "auto")) != "box-none" ? Array(p_state.props.get("godotContextMenu", Array())) : Array();
	if (entries == published_entries) {
		return;
	}
	published_entries = entries.duplicate(true);
	if (published_entries.is_empty() && !menu) {
		return;
	}
	if (!menu) {
		menu = memnew(PopupMenu);
		add_child(menu);
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
	if (p_id < 0 || p_id >= published_entries.size() || !published_context.event_sink || !published_context.event_sink->emit) {
		return;
	}
	const Dictionary entry = published_entries[p_id];
	if (bool(entry.get("disabled", false)) || bool(entry.get("separator", false))) {
		return;
	}
	Dictionary payload;
	payload["id"] = entry["id"];
	published_context.event_sink->emit(published_context.tag, "topGodotContextMenuAction", payload, published_context.revision);
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
		if (published_context.event_sink && published_context.event_sink->invalidate_geometry) {
			published_context.event_sink->invalidate_geometry();
		}
		menu->popup(Rect2i(Point2i(position), Size2i()));
	}
}

std::shared_ptr<const RNHostDescriptor> rn_view_descriptor() {
	return std::make_shared<RNViewDescriptor>();
}
