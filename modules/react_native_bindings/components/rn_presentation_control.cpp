#include "rn_presentation_control.h"

#include "../fabric/rn_shadow_node.h"
#include "../fabric/rn_view_style.h"
#include "../root_view/react_native_root_view.h"
#include "rn_text_input_control.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"

#include <map>

namespace {
Dictionary presentation_config(bool p_window) {
	Dictionary events;
	for (const char *name : { "Show", "Dismiss", "RequestClose" }) {
		Dictionary registration;
		registration["registrationName"] = String("on") + name;
		events[String("top") + name] = registration;
	}
	if (p_window) {
		Dictionary registration;
		registration["registrationName"] = "onResize";
		events["topResize"] = registration;
	}
	Dictionary config;
	config["directEventTypes"] = events;
	return config;
}
class RNPresentationDescriptor : public RNHostDescriptor {
	bool native_window;

public:
	RNPresentationDescriptor(bool p_window) : RNHostDescriptor(p_window ? "GodotWindow" : "RCTModalHostView", RNHostTraits{ true, true, true, false, false, false, false, true, true, true, true }, presentation_config(p_window)), native_window(p_window) {}
	bool prepare(const RNShadowNode &p_node, RNPreparedHostState &r_state, RNError &r_error) const override {
		if (!RNHostDescriptor::prepare(p_node, r_state, r_error)) {
			return false;
		}
		auto component = std::make_shared<RNPresentationData>();
		if (native_window) {
			const double width = r_state.props.get("windowWidth", 640);
			const double height = r_state.props.get("windowHeight", 480);
			if (!Math::is_finite(width) || !Math::is_finite(height) || width < 1 || height < 1 || width > 16384 || height > 16384) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid native window size", "window.prepare");
				return false;
			}
			component->viewport = Size2(width, height);
		}
		r_state.component_data = component;
		return true;
	}
	bool resolve_resources(RNPreparedHostState &r_state, const RNHostContext &p_context, RNError &) const override {
		auto component = std::make_shared<RNPresentationData>(*std::static_pointer_cast<const RNPresentationData>(r_state.component_data));
		if (!native_window) {
			component->viewport = p_context.presentation_size;
		} else if (auto *host = Object::cast_to<RNWindowControl>(ObjectDB::get_instance(p_context.host_id))) {
			component->viewport = host->resolve_size(component->viewport);
		}
		r_state.component_data = component;
		r_state.dependency_revision = uint64_t(component->viewport.x * 65536 + component->viewport.y);
		return true;
	}
	Size2 presentation_size(const RNPreparedHostState &p_state, const Size2 &p_inherited) const override { return native_window ? std::static_pointer_cast<const RNPresentationData>(p_state.component_data)->viewport : p_inherited; }
	Control *create_host(const RNHostContext &) const override { return native_window ? static_cast<Control *>(memnew(RNWindowControl)) : static_cast<Control *>(memnew(RNModalControl)); }
	Control *get_child_container(Control *p_host, const RNHostContext &) const override { return native_window ? Object::cast_to<RNWindowControl>(p_host)->get_content() : Object::cast_to<RNModalControl>(p_host)->get_overlay(); }
	RNChildLayoutPolicy get_child_layout_policy(const RNPreparedHostState &) const override { return RNChildLayoutPolicy::PRESENTATION; }
	Rect2 get_child_layout_viewport(const RNPreparedHostState &p_state, const Size2 &) const override { return Rect2(Point2(), std::static_pointer_cast<const RNPresentationData>(p_state.component_data)->viewport); }
	bool apply(Control *, const RNPreparedHostState &, const RNHostContext &, RNError &) const override { return true; }
	Variant capture_state(Control *) const override { return Dictionary(); }
	void restore_state(Control *, const Variant &) const override {}
	void dispose_state(Control *p_host, const RNHostContext &) const override {
		if (native_window) {
			Object::cast_to<RNWindowControl>(p_host)->get_native_window()->hide();
		} else {
			Object::cast_to<RNModalControl>(p_host)->dismiss();
		}
	}
	void after_publish(Control *p_host, const RNPreparedHostState &p_state, const RNHostContext &p_context) const override {
		if (native_window) {
			Object::cast_to<RNWindowControl>(p_host)->publish(p_state, p_context);
		} else {
			Object::cast_to<RNModalControl>(p_host)->publish(p_state, p_context);
		}
	}
	RNHostGeometry read_geometry(Control *p_host, const RNHostContext &p_context) const override {
		Control *content = get_child_container(p_host, p_context);
		auto geometry = RNHostDescriptor::read_geometry(content, p_context);
		geometry.viewport = Rect2(Point2(), content->get_size());
		geometry.clips_contents = true;
		return geometry;
	}
	bool owns_input_control(Control *p_host, Control *p_control) const override { return p_host == p_control || get_child_container(p_host, RNHostContext()) == p_control; }
};
Control *presentation_container(RNModalControl *p_modal, const RNHostContext &p_context) {
	Control *container = p_context.owner;
	for (Node *node = p_modal->get_parent(); node && node != p_context.owner; node = node->get_parent()) {
		if (Object::cast_to<Window>(node)) {
			break;
		}
		if (auto *control = Object::cast_to<Control>(node)) {
			container = control;
		}
	}
	return container ? container : p_modal;
}
std::shared_ptr<RNModalDomain> modal_domain(RNModalControl *p_modal, const RNHostContext &p_context, Control *p_container) {
	static std::map<String, std::weak_ptr<RNModalDomain>> domains;
	for (auto it = domains.begin(); it != domains.end();) {
		if (it->second.expired()) {
			it = domains.erase(it);
		} else {
			++it;
		}
	}
	const String key = String::num_uint64(p_context.generation) + ":" + String::num_int64(p_context.root_tag) + ":" + String::num_uint64(p_context.surface_epoch) + ":" + String::num_uint64(uint64_t(p_modal->get_viewport()->get_instance_id()));
	auto domain = domains[key].lock();
	if (!domain) {
		domain = std::make_shared<RNModalDomain>();
		domain->container = p_container->get_instance_id();
		domains[key] = domain;
	}
	return domain;
}
void focusable_controls(Node *p_node, Vector<Control *> &r_controls) {
	if (auto *control = Object::cast_to<Control>(p_node)) {
		if (control->is_visible_in_tree() && (control->get_focus_mode() == Control::FOCUS_ALL || control->get_focus_mode() == Control::FOCUS_CLICK)) {
			r_controls.push_back(control);
		}
	}
	for (int i = 0; i < p_node->get_child_count(); ++i) {
		if (!Object::cast_to<Window>(p_node->get_child(i))) {
			focusable_controls(p_node->get_child(i), r_controls);
		}
	}
}
} //namespace
RNModalControl::RNModalControl() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	overlay = memnew(Panel);
	overlay_id = overlay->get_instance_id();
	overlay->set_as_top_level(true);
	overlay->set_mouse_filter(MOUSE_FILTER_STOP);
	overlay->set_clip_contents(true);
	overlay->set_focus_mode(FOCUS_ALL);
	overlay->hide();
	add_child(overlay);
	set_process_input(true);
}
RNModalControl::~RNModalControl() {
	_remove_presentation();
	if (ObjectDB::get_instance(overlay_id) && overlay->get_parent() != this) {
		memdelete(overlay);
	}
}
bool RNModalControl::is_top() const {
	return domain && !domain->stack.is_empty() && domain->stack[domain->stack.size() - 1] == get_instance_id();
}
void RNModalControl::_remove_presentation() {
	if (!presented || !domain) {
		return;
	}
	const bool overlay_alive = ObjectDB::get_instance(overlay_id) != nullptr;
	const bool top = is_top();
	Control *focus_owner = is_inside_tree() ? get_viewport()->gui_get_focus_owner() : nullptr;
	const bool owns_focus = overlay_alive && focus_owner && (focus_owner == overlay || overlay->is_ancestor_of(focus_owner));
	domain->stack.erase(get_instance_id());
	presented = false;
	if (overlay_alive) {
		overlay->hide();
	}
	if (top && owns_focus) {
		Control *target = nullptr;
		if (!domain->stack.is_empty()) {
			auto *modal = Object::cast_to<RNModalControl>(ObjectDB::get_instance(domain->stack[domain->stack.size() - 1]));
			if (modal) {
				target = Object::cast_to<Control>(ObjectDB::get_instance(modal->last_focus));
				if (!target || !modal->overlay->is_ancestor_of(target)) {
					target = modal->overlay;
				}
			}
		} else {
			target = Object::cast_to<Control>(ObjectDB::get_instance(domain->saved_focus));
			auto *container = Object::cast_to<Control>(ObjectDB::get_instance(domain->container));
			if (!container || !target || (container != target && !container->is_ancestor_of(target))) {
				target = nullptr;
			}
		}
		if (target && target->is_inside_tree() && target->is_visible_in_tree() && target->get_focus_mode() != FOCUS_NONE && !target->get_viewport()->gui_get_focus_owner()) {
			target->grab_focus();
		}
	}
}
void RNModalControl::_update_focus_cycle() {
	Vector<Control *> controls;
	focusable_controls(overlay, controls);
	for (int i = 0; i < controls.size(); ++i) {
		controls[i]->set_focus_next(controls[i]->get_path_to(controls[(i + 1) % controls.size()]));
		controls[i]->set_focus_previous(controls[i]->get_path_to(controls[(i + controls.size() - 1) % controls.size()]));
	}
}
void RNModalControl::publish(const RNPreparedHostState &p_state, const RNHostContext &p_context) {
	published_context = p_context;
	const bool should_show = p_state.props.get("visible", true);
	const auto component = std::static_pointer_cast<const RNPresentationData>(p_state.component_data);
	const bool showing = should_show && !presented;
	if (!should_show) {
		const bool dismissing = presented;
		_remove_presentation();
		if (dismissing && p_context.event_sink && p_context.event_sink->emit) {
			p_context.event_sink->emit(p_context.tag, "topDismiss", Dictionary(), p_context.revision);
		}
		return;
	}
	Control *container = presentation_container(this, p_context);
	if (overlay->get_parent() != container) {
		overlay->reparent(container, false);
	}
	overlay->set_position(Point2());
	overlay->set_external_layout_enabled(true);
	overlay->set_size(component->viewport);
	Dictionary backdrop;
	backdrop["backgroundColor"] = bool(p_state.props.get("transparent", false)) ? Variant(Color(0, 0, 0, 0)) : p_state.props.get("backdropColor", Variant(Color(1, 1, 1, 1)));
	overlay->add_theme_style_override("panel", RNViewStyle::build_stylebox(backdrop));
	if (showing) {
		domain = modal_domain(this, p_context, container);
		Control *focus_owner = get_viewport()->gui_get_focus_owner();
		const bool owns_focus = !focus_owner || focus_owner == container || container->is_ancestor_of(focus_owner);
		if (domain->stack.is_empty()) {
			domain->saved_focus = owns_focus && focus_owner ? focus_owner->get_instance_id() : ObjectID();
		} else {
			auto *previous = Object::cast_to<RNModalControl>(ObjectDB::get_instance(domain->stack[domain->stack.size() - 1]));
			if (previous && focus_owner && (previous->overlay == focus_owner || previous->overlay->is_ancestor_of(focus_owner))) {
				previous->last_focus = focus_owner->get_instance_id();
			}
		}
		domain->stack.push_back(get_instance_id());
		presented = true;
		container->move_child(overlay, container->get_child_count() - 1);
		overlay->show();
		if (owns_focus) {
			overlay->grab_focus();
		}
		if (p_context.event_sink && p_context.event_sink->cancel_input) {
			p_context.event_sink->cancel_input();
		}
		if (p_context.event_sink && p_context.event_sink->emit) {
			p_context.event_sink->emit(p_context.tag, "topShow", Dictionary(), p_context.revision);
		}
	} else {
		overlay->show();
	}
	_update_focus_cycle();
	if (p_context.event_sink && p_context.event_sink->invalidate_geometry) {
		p_context.event_sink->invalidate_geometry();
	}
}
void RNModalControl::request_close() {
	if (is_top() && published_context.event_sink && published_context.event_sink->emit) {
		published_context.event_sink->emit(published_context.tag, "topRequestClose", Dictionary(), published_context.revision);
	}
}
void RNModalControl::input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == Key::ESCAPE && is_top()) {
		Control *focus_owner = get_viewport()->gui_get_focus_owner();
		if (focus_owner && (focus_owner == overlay || overlay->is_ancestor_of(focus_owner))) {
			request_close();
			get_viewport()->set_input_as_handled();
		}
	}
}
RNWindowControl::RNWindowControl() {
	set_mouse_filter(MOUSE_FILTER_IGNORE);
	window = memnew(Window);
	window->hide();
	window->set_title("React Native");
	window->set_transient(true);
	add_child(window);
	content = memnew(Control);
	content->set_mouse_filter(MOUSE_FILTER_PASS);
	window->add_child(content);
	window->connect("close_requested", callable_mp(this, &RNWindowControl::_close_requested));
	window->connect("size_changed", callable_mp(this, &RNWindowControl::_resized));
}
Size2 RNWindowControl::resolve_size(const Size2 &p_declared) const {
	return declared_size == p_declared ? Size2(window->get_size()) : p_declared;
}
void RNWindowControl::publish(const RNPreparedHostState &p_state, const RNHostContext &p_context) {
	published_context = p_context;
	const auto component = std::static_pointer_cast<const RNPresentationData>(p_state.component_data);
	window->set_title(p_state.props.get("title", "React Native"));
	publishing = true;
	window->set_size(component->viewport);
	declared_size = Size2(p_state.props.get("windowWidth", 640), p_state.props.get("windowHeight", 480));
	content->set_external_layout_enabled(true);
	content->set_size(component->viewport);
	publishing = false;
	const bool should_show = p_state.props.get("visible", true);
	const bool changed = window->is_visible() != should_show;
	window->set_visible(should_show);
	if (changed && p_context.event_sink && p_context.event_sink->emit) {
		p_context.event_sink->emit(p_context.tag, should_show ? "topShow" : "topDismiss", Dictionary(), p_context.revision);
	}
	if (p_context.owner) {
		const Callable observer = callable_mp(p_context.owner, &ReactNativeRootView::_on_gui_input_dispatched);
		if (!window->is_connected("gui_input_dispatched", observer)) {
			window->connect("gui_input_dispatched", observer);
		}
	}
	if (p_context.event_sink && p_context.event_sink->invalidate_geometry) {
		p_context.event_sink->invalidate_geometry();
	}
}
void RNWindowControl::_close_requested() {
	if (published_context.event_sink && published_context.event_sink->emit) {
		published_context.event_sink->emit(published_context.tag, "topRequestClose", Dictionary(), published_context.revision);
	}
}
void RNWindowControl::_resized() {
	if (publishing) {
		return;
	}
	content->set_size(window->get_size());
	if (published_context.event_sink) {
		if (published_context.event_sink->invalidate_layout) {
			published_context.event_sink->invalidate_layout();
		}
		if (published_context.event_sink->emit) {
			Dictionary size;
			size["width"] = window->get_size().x;
			size["height"] = window->get_size().y;
			published_context.event_sink->emit(published_context.tag, "topResize", size, published_context.revision);
		}
	}
}
std::shared_ptr<const RNHostDescriptor> rn_modal_descriptor() {
	return std::make_shared<RNPresentationDescriptor>(false);
}
std::shared_ptr<const RNHostDescriptor> rn_window_descriptor() {
	return std::make_shared<RNPresentationDescriptor>(true);
}
