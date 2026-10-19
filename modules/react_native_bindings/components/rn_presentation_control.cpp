#include "rn_presentation_control.h"

#include "../root_view/react_native_root_view.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/style_box_flat.h"
#include "servers/display/accessibility_server.h"

#include <map>

namespace {
ReactNativeRootView *owning_root(Node *p_node) {
	for (Node *node = p_node; node; node = node->get_parent()) {
		if (auto *root = Object::cast_to<ReactNativeRootView>(node)) {
			return root;
		}
	}
	return nullptr;
}
Control *presentation_container(RNModalControl *p_modal) {
	ReactNativeRootView *root = owning_root(p_modal);
	Control *container = root;
	for (Node *node = p_modal->get_parent(); node && node != root; node = node->get_parent()) {
		if (Object::cast_to<Window>(node)) {
			break;
		}
		if (auto *control = Object::cast_to<Control>(node)) {
			container = control;
		}
	}
	return container ? container : p_modal;
}
std::shared_ptr<RNModalDomain> modal_domain(RNModalControl *p_modal, Control *p_container) {
	static std::map<String, std::weak_ptr<RNModalDomain>> domains;
	for (auto it = domains.begin(); it != domains.end();) {
		if (it->second.expired()) {
			it = domains.erase(it);
		} else {
			++it;
		}
	}
	const String key = String::num_uint64(uint64_t(p_container->get_instance_id())) + ":" + String::num_uint64(uint64_t(p_modal->get_viewport()->get_instance_id()));
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
	overlay = memnew(RNModalOverlay);
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
void RNModalControl::request_presentation(bool p_visible, const Color &p_backdrop) {
	requested_visible = p_visible;
	requested_backdrop = p_backdrop;
	if (is_inside_tree()) {
		_apply_request();
	}
}
// A native sub-window reports an empty visible rect until it is shown, so its
// own size is the area a modal covers.
Size2 RNModalControl::get_presentation_size() const {
	Window *native_window = Object::cast_to<Window>(get_viewport());
	if (native_window && native_window != get_tree()->get_root()) {
		return native_window->get_size();
	}
	return get_viewport()->get_visible_rect().size;
}
void RNModalControl::_apply_request() {
	if (is_inside_tree()) {
		present(requested_visible, requested_backdrop, get_presentation_size());
	}
}
// Presenting reparents the overlay, which the parent forbids while it is
// still adding this modal.
void RNModalOverlay::_notification(int p_what) {
	if (p_what == NOTIFICATION_ACCESSIBILITY_UPDATE) {
		const RID element = get_accessibility_element();
		ERR_FAIL_COND(element.is_null());
		AccessibilityServer::get_singleton()->update_set_role(element, AccessibilityServerEnums::ROLE_DIALOG);
		AccessibilityServer::get_singleton()->update_set_flag(element, AccessibilityServerEnums::FLAG_MODAL, true);
	}
}

void RNModalControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE && requested_visible) {
		callable_mp(this, &RNModalControl::_apply_request).call_deferred();
	}
}
void RNModalControl::present(bool p_visible, const Color &p_backdrop, const Size2 &p_viewport) {
	const bool showing = p_visible && !presented;
	if (!p_visible) {
		const bool dismissing = presented;
		_remove_presentation();
		if (dismissing && callbacks.dismiss) {
			callbacks.dismiss();
		}
		return;
	}
	Control *container = presentation_container(this);
	if (overlay->get_parent() != container) {
		overlay->reparent(container, false);
	}
	overlay->set_position(Point2());
	overlay->set_size(p_viewport);
	Ref<StyleBoxFlat> backdrop;
	backdrop.instantiate();
	backdrop->set_bg_color(p_backdrop);
	overlay->add_theme_style_override("panel", backdrop);
	if (showing) {
		domain = modal_domain(this, container);
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
		if (callbacks.presented) {
			callbacks.presented();
		}
		if (callbacks.show) {
			callbacks.show();
		}
	} else {
		overlay->show();
	}
	_update_focus_cycle();
	if (callbacks.geometry_changed) {
		callbacks.geometry_changed();
	}
}
void RNModalControl::request_close() {
	if (is_top() && callbacks.request_close) {
		callbacks.request_close();
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
	content = memnew(RNWindowContent);
	content->set_mouse_filter(MOUSE_FILTER_PASS);
	window->add_child(content);
	window->connect("close_requested", callable_mp(this, &RNWindowControl::_close_requested));
	window->connect("size_changed", callable_mp(this, &RNWindowControl::_resized));
}
Size2 RNWindowControl::resolve_size(const Size2 &p_declared) const {
	return declared_size == p_declared ? Size2(window->get_size()) : p_declared;
}
void RNWindowControl::present(const String &p_title, const Size2 &p_size, const Size2 &p_declared_size, bool p_visible) {
	window->set_title(p_title);
	publishing = true;
	window->set_size(p_size);
	declared_size = p_declared_size;
	content->set_size(p_size);
	publishing = false;
	const bool changed = window->is_visible() != p_visible;
	window->set_visible(p_visible);
	if (changed && p_visible && callbacks.show) {
		callbacks.show();
	} else if (changed && !p_visible && callbacks.dismiss) {
		callbacks.dismiss();
	}
	if (callbacks.geometry_changed) {
		callbacks.geometry_changed();
	}
}
// The window's input bypasses the root's viewport, so the root sees it here.
void RNWindowControl::_notification(int p_what) {
	if (p_what != NOTIFICATION_ENTER_TREE) {
		return;
	}
	ReactNativeRootView *root = owning_root(this);
	if (root && root->get_instance_id() != observed_root) {
		observed_root = root->get_instance_id();
		window->connect("window_input", callable_mp(root, &ReactNativeRootView::_begin_input).bind(window->get_instance_id()));
	}
}
void RNWindowControl::_close_requested() {
	if (callbacks.request_close) {
		callbacks.request_close();
	}
}
void RNWindowControl::_resized() {
	if (publishing) {
		return;
	}
	content->set_size(window->get_size());
	if (callbacks.resized) {
		callbacks.resized(window->get_size());
	}
}
