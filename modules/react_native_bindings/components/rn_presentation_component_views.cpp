#include "rn_presentation_component_views.h"

#include "scene/main/viewport.h"

#include <react/renderer/components/FBReactNativeSpec/EventEmitters.h>
#include <react/renderer/components/FBReactNativeSpec/Props.h>
#include <react/renderer/components/godotwindow/GodotWindowShadowNode.h>
#include <react/renderer/components/modal/ModalHostViewState.h>
#include <react/renderer/core/ConcreteState.h>

using namespace facebook::react;

RNModalComponentView::RNModalComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNModalControl)), modal(Object::cast_to<RNModalControl>(get_control())) {
}

void RNModalComponentView::_report_screen_size() {
	auto concrete = std::static_pointer_cast<const ConcreteState<ModalHostViewState>>(state);
	if (!concrete || !modal->is_inside_tree()) {
		return;
	}
	const Size2 viewport = modal->get_viewport()->get_visible_rect().size;
	const Size screen{ .width = viewport.x, .height = viewport.y };
	if (concrete->getData().screenSize != screen) {
		concrete->updateState(ModalHostViewState(screen));
	}
}

void RNModalComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	props = std::static_pointer_cast<const ViewProps>(p_new);
	const auto &next = static_cast<const ModalHostViewProps &>(*p_new);
	const Size2 viewport = modal->is_inside_tree() ? modal->get_viewport()->get_visible_rect().size : Size2();
	modal->present(next.visible, ::Color(0, 0, 0, 0), viewport);
}

void RNModalComponentView::update_state(const State::Shared &p_state) {
	RNComponentView::update_state(p_state);
	_report_screen_size();
}

void RNModalComponentView::update_layout(const LayoutMetrics &p_metrics) {
	layout_metrics = p_metrics;
	_report_screen_size();
}

void RNModalComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const ModalHostViewEventEmitter>(p_emitter);
	RNPresentationCallbacks callbacks;
	if (emitter) {
		callbacks.show = [emitter]() { emitter->onShow({}); };
		callbacks.dismiss = [emitter]() { emitter->onDismiss({}); };
		callbacks.request_close = [emitter]() { emitter->onRequestClose({}); };
	}
	modal->set_callbacks(callbacks);
}

RNWindowComponentView::RNWindowComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNWindowControl)), window(Object::cast_to<RNWindowControl>(get_control())) {
}

void RNWindowComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	props = std::static_pointer_cast<const ViewProps>(p_new);
	const auto &next = static_cast<const GodotWindowProps &>(*p_new);
	const Size2 declared(next.windowWidth, next.windowHeight);
	window->present(String::utf8(next.title.c_str()), window->resolve_size(declared), declared, next.visible);
}

void RNWindowComponentView::update_layout(const LayoutMetrics &p_metrics) {
	layout_metrics = p_metrics;
}

void RNWindowComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const GodotWindowEventEmitter>(p_emitter);
	RNPresentationCallbacks callbacks;
	if (emitter) {
		callbacks.show = [emitter]() { emitter->onShow({}); };
		callbacks.dismiss = [emitter]() { emitter->onDismiss({}); };
		callbacks.request_close = [emitter]() { emitter->onRequestClose({}); };
		callbacks.resized = [this, emitter](const Size2 &p_size) {
			if (auto concrete = std::static_pointer_cast<const ConcreteState<GodotWindowState>>(state)) {
				concrete->updateState(GodotWindowState{ .windowSize = { .width = p_size.x, .height = p_size.y } });
			}
			emitter->onResize({ .width = p_size.x, .height = p_size.y });
		};
	}
	window->set_callbacks(callbacks);
}
