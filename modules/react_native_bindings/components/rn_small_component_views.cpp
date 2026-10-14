#include "rn_small_component_views.h"

#include <react/renderer/components/FBReactNativeSpec/EventEmitters.h>
#include <react/renderer/components/FBReactNativeSpec/Props.h>

using namespace facebook::react;

RNSwitchComponentView::RNSwitchComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNSwitchControl)), toggle(Object::cast_to<RNSwitchControl>(get_control())) {
}

void RNSwitchComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const SwitchProps &>(*p_new);
	toggle->set_pressed_no_signal(next.value);
	toggle->set_disabled(next.disabled);
	const bool targetable = next.pointerEvents != PointerEventsMode::None && next.pointerEvents != PointerEventsMode::BoxNone;
	toggle->set_mouse_filter(targetable ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_IGNORE);
	toggle->set_self_modulate(rn_godot_color(next.thumbTintColor ? next.thumbTintColor : next.thumbColor, ::Color(1, 1, 1)));
}

void RNSwitchComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const SwitchEventEmitter>(p_emitter);
	const Tag target = tag;
	toggle->set_changed_callback(emitter ? std::function<void(bool)>([emitter, target](bool p_value) { emitter->onChange({ .value = p_value, .target = target }); }) : nullptr);
}

void RNSwitchComponentView::handle_command(const std::string &p_name, const folly::dynamic &p_args) {
	if (p_name == "setValue" && p_args.size() == 1) {
		toggle->set_pressed_no_signal(p_args[0].asBool());
	}
}

RNActivityIndicatorComponentView::RNActivityIndicatorComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNActivityIndicatorControl)), indicator(Object::cast_to<RNActivityIndicatorControl>(get_control())) {
}

void RNActivityIndicatorComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const ActivityIndicatorViewProps &>(*p_new);
	indicator->set_indeterminate(next.animating);
	::Color tint = rn_godot_color(next.color, ::Color(1, 1, 1));
	tint.a *= next.opacity;
	indicator->set_modulate(tint);
	indicator->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	update_layout(layout_metrics);
}

void RNActivityIndicatorComponentView::update_layout(const LayoutMetrics &p_metrics) {
	RNComponentView::update_layout(p_metrics);
	const auto next = std::static_pointer_cast<const ActivityIndicatorViewProps>(props);
	if (next && !next->animating && next->hidesWhenStopped) {
		indicator->set_visible(false);
	}
}
