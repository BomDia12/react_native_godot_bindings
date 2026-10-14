#include "rn_view_component_view.h"

#include <react/renderer/components/view/ViewEventEmitter.h>

RNViewComponentView::RNViewComponentView(facebook::react::Tag p_tag) :
		RNComponentView(p_tag, memnew(RNViewControl)), view(Object::cast_to<RNViewControl>(get_control())) {
}

void RNViewComponentView::_apply_style() {
	view->add_theme_style_override(SNAME("panel"), rn_view_stylebox(*props, layout_metrics));
}

void RNViewComponentView::update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	using facebook::react::PointerEventsMode;
	const bool self_targetable = props->pointerEvents != PointerEventsMode::None && props->pointerEvents != PointerEventsMode::BoxNone;
	view->set_focus_mode(self_targetable && props->focusable ? Control::FOCUS_ALL : Control::FOCUS_NONE);
	view->set_menu_entries(self_targetable ? Array(rn_variant_from_dynamic(props->godotContextMenu)) : Array());
	Dictionary accessibility;
	accessibility["accessible"] = props->accessible;
	accessibility["accessibilityLabel"] = String::utf8(props->accessibilityLabel.c_str());
	accessibility["accessibilityHint"] = String::utf8(props->accessibilityHint.c_str());
	accessibility["accessibilityElementsHidden"] = props->accessibilityElementsHidden;
	view->set_meta("react_native_accessibility", accessibility);
	_apply_style();
}

void RNViewComponentView::update_layout(const facebook::react::LayoutMetrics &p_metrics) {
	RNComponentView::update_layout(p_metrics);
	if (props) {
		_apply_style();
	}
}

void RNViewComponentView::update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const facebook::react::ViewEventEmitter>(p_emitter);
	if (!emitter) {
		view->set_menu_callbacks(nullptr, nullptr);
		view->set_focus_callback(nullptr);
		return;
	}
	view->set_menu_callbacks([emitter](const String &p_id) { emitter->onGodotContextMenuAction(p_id.utf8().get_data()); }, nullptr);
	view->set_focus_callback([emitter](bool p_focused) {
		if (p_focused) {
			emitter->onFocus();
		} else {
			emitter->onBlur();
		}
	});
}

void RNViewComponentView::handle_command(const std::string &p_name, const folly::dynamic &) {
	if (p_name == "focus") {
		view->grab_focus();
	} else if (p_name == "blur") {
		view->release_focus();
	}
}
