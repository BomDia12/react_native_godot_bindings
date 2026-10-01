#include "rn_builtin_descriptors.h"

#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"

#include "core/object/callable_mp.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/resources/font.h"
#include "scene/theme/theme_db.h"

namespace {

bool validate_color_prop(const Dictionary &p_props, const String &p_name, RNError &r_error, const String &p_operation) {
	if (!p_props.has(p_name)) {
		return true;
	}
	Color ignored;
	if (RNViewStyle::color_of(p_props, p_name, ignored)) {
		return true;
	}
	r_error = RNError::make(RNErrorCode::VALIDATION, vformat("%s must be a finite Godot Color with channels in 0-1", p_name), p_operation, "props." + p_name);
	return false;
}

void apply_common(Control *p_host, const RNPreparedHostState &p_state) {
	p_host->set_visible(String(p_state.props.get("display", "flex")) != "none");
	p_host->set_modulate(Color(1, 1, 1, RNViewStyle::opacity_of(p_state.props)));
}

class RNRootDescriptor : public RNHostDescriptor {
public:
	RNRootDescriptor() :
			RNHostDescriptor("RCTRootView", RNHostTraits{ false, true, true, false, false, false, false, false, true, false, true }) {}
};

class RNRawTextDescriptor : public RNHostDescriptor {
public:
	RNRawTextDescriptor() :
			RNHostDescriptor("RCTRawText", RNHostTraits{ false, false, false, false, true, false, false, false, false, false, true }) {}
};

class RNViewDescriptor : public RNHostDescriptor {
public:
	RNViewDescriptor() :
			RNHostDescriptor("RCTView", RNHostTraits{ true, true, true, false, false, false, true, true, true, true, true }) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		return RNHostDescriptor::prepare(p_node, r_state, r_error) &&
				validate_color_prop(r_state.props, "backgroundColor", r_error, "RCTView.prepare") &&
				validate_color_prop(r_state.props, "borderColor", r_error, "RCTView.prepare");
	}

	Control *create_host(const RNHostContext &p_context) const override {
		(void)p_context;
		return memnew(Panel);
	}

	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context, RNError &r_error) const override {
		(void)p_context;
		Panel *panel = Object::cast_to<Panel>(p_host);
		if (!panel) {
			r_error = RNError::make(RNErrorCode::NATIVE, "RCTView host is not a Panel", "RCTView.apply");
			return false;
		}
		apply_common(panel, p_state);
		panel->add_theme_style_override("panel", RNViewStyle::build_stylebox(p_state.props));
		panel->set_clip_contents(RNViewStyle::clips_contents(p_state.props));
		const String pointer_events = String(p_state.props.get("pointerEvents", "auto")).to_lower();
		const bool branch_enabled = p_state.branch_targetable && pointer_events != "none";
		const bool self_targetable = branch_enabled && pointer_events != "box-none";
		panel->set_mouse_filter(branch_enabled ? Control::MOUSE_FILTER_PASS : Control::MOUSE_FILTER_IGNORE);
		panel->set_focus_mode(self_targetable && bool(p_state.props.get("focusable", false)) ? Control::FOCUS_ALL : Control::FOCUS_NONE);
		return true;
	}

	Variant capture_state(Control *p_host) const override {
		Panel *panel = Object::cast_to<Panel>(p_host);
		if (!panel) {
			return Variant();
		}
		Dictionary state;
		state["visible"] = panel->is_visible();
		state["modulate"] = panel->get_modulate();
		state["style"] = panel->get_theme_stylebox("panel");
		state["clip"] = panel->is_clipping_contents();
		state["mouse"] = int(panel->get_mouse_filter());
		state["focus"] = int(panel->get_focus_mode());
		return state;
	}

	void restore_state(Control *p_host, const Variant &p_state) const override {
		Panel *panel = Object::cast_to<Panel>(p_host);
		if (!panel || p_state.get_type() != Variant::DICTIONARY) {
			return;
		}
		const Dictionary state = p_state;
		panel->set_visible(state.get("visible", true));
		panel->set_modulate(state.get("modulate", Color(1, 1, 1, 1)));
		const Ref<StyleBox> style = state.get("style", Ref<StyleBox>());
		if (style.is_valid()) {
			panel->add_theme_style_override("panel", style);
		}
		panel->set_clip_contents(state.get("clip", false));
		panel->set_mouse_filter(Control::MouseFilter(int(state.get("mouse", int(Control::MOUSE_FILTER_PASS)))));
		panel->set_focus_mode(Control::FocusMode(int(state.get("focus", int(Control::FOCUS_NONE)))));
	}

	void attach_signals(Control *p_host, const RNHostContext &p_context) const override {
		if (!p_context.owner) {
			return;
		}
		const Callable entered = callable_mp(p_context.owner, &ReactNativeRootView::_on_focus_entered).bind(p_context.tag, p_host->get_instance_id());
		const Callable exited = callable_mp(p_context.owner, &ReactNativeRootView::_on_focus_exited).bind(p_context.tag, p_host->get_instance_id());
		if (!p_host->is_connected("focus_entered", entered)) {
			p_host->connect("focus_entered", entered);
		}
		if (!p_host->is_connected("focus_exited", exited)) {
			p_host->connect("focus_exited", exited);
		}
	}

	bool dispatch_command(Control *p_host, const StringName &p_command, const Variant &p_arguments, const RNHostContext &p_context, RNError &r_error) const override {
		(void)p_arguments;
		(void)p_context;
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
};

class RNTextDescriptor : public RNHostDescriptor {
public:
	RNTextDescriptor() :
			RNHostDescriptor("RCTText", RNHostTraits{ true, true, false, true, false, true, false, false, false, true, true }) {}

	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		return RNHostDescriptor::prepare(p_node, r_state, r_error) && validate_color_prop(r_state.props, "color", r_error, "RCTText.prepare");
	}

	Control *create_host(const RNHostContext &p_context) const override {
		(void)p_context;
		return memnew(Label);
	}

	bool apply(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context, RNError &r_error) const override {
		(void)p_context;
		Label *label = Object::cast_to<Label>(p_host);
		if (!label) {
			r_error = RNError::make(RNErrorCode::NATIVE, "RCTText host is not a Label", "RCTText.apply");
			return false;
		}
		apply_common(label, p_state);
		label->set_text(p_state.text);
		float font_size = 0;
		if (RNViewStyle::font_size_of(p_state.props, font_size)) {
			label->add_theme_font_size_override("font_size", int(font_size));
		} else {
			label->remove_theme_font_size_override("font_size");
		}
		Color color;
		if (RNViewStyle::color_of(p_state.props, "color", color)) {
			label->add_theme_color_override("font_color", color);
		} else {
			label->remove_theme_color_override("font_color");
		}
		return true;
	}

	Size2 measure(const RNPreparedHostState &p_state, const RNMeasureConstraints &p_constraints) const override {
		const Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
		if (font.is_null()) {
			return Size2();
		}
		float font_size = ThemeDB::get_singleton()->get_fallback_font_size();
		RNViewStyle::font_size_of(p_state.props, font_size);
		const float wrap_width = p_constraints.width_mode == RNMeasureMode::UNDEFINED ? -1 : p_constraints.width;
		Size2 measured = font->get_multiline_string_size(p_state.text, HORIZONTAL_ALIGNMENT_LEFT, wrap_width, font_size);
		if (p_constraints.width_mode == RNMeasureMode::EXACTLY) {
			measured.x = p_constraints.width;
		} else if (p_constraints.width_mode == RNMeasureMode::AT_MOST) {
			measured.x = MIN(measured.x, p_constraints.width);
		}
		if (p_constraints.height_mode == RNMeasureMode::EXACTLY) {
			measured.y = p_constraints.height;
		} else if (p_constraints.height_mode == RNMeasureMode::AT_MOST) {
			measured.y = MIN(measured.y, p_constraints.height);
		}
		return measured;
	}
};

} // namespace

bool rn_register_builtin_descriptors(RNHostDescriptorRegistry &p_registry, RNError &r_error) {
	return p_registry.register_descriptor(std::make_shared<RNRootDescriptor>(), r_error) &&
			p_registry.register_descriptor(std::make_shared<RNViewDescriptor>(), r_error) &&
			p_registry.register_descriptor(std::make_shared<RNTextDescriptor>(), r_error) &&
			p_registry.register_descriptor(std::make_shared<RNRawTextDescriptor>(), r_error);
}
