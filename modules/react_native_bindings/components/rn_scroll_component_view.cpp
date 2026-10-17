#include "rn_scroll_component_view.h"

#include <react/renderer/components/scrollview/ScrollViewEventEmitter.h>
#include <react/renderer/components/scrollview/ScrollViewProps.h>
#include <react/renderer/components/scrollview/ScrollViewState.h>
#include <react/renderer/core/ConcreteState.h>

using namespace facebook::react;

RNScrollComponentView::RNScrollComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNScrollControl)), scroll(Object::cast_to<RNScrollControl>(get_control())) {
}

void RNScrollComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const ScrollViewProps &>(*p_new);
	RNScrollSettings settings;
	settings.horizontal = next.horizontal;
	settings.paging = next.pagingEnabled;
	settings.snap_interval = next.snapToInterval;
	for (Float offset : next.snapToOffsets) {
		settings.snap_offsets.push_back(offset);
	}
	settings.snap_alignment = next.snapToAlignment == ScrollViewSnapToAlignment::Center ? HORIZONTAL_ALIGNMENT_CENTER : next.snapToAlignment == ScrollViewSnapToAlignment::End ? HORIZONTAL_ALIGNMENT_RIGHT
																																											   : HORIZONTAL_ALIGNMENT_LEFT;
	settings.maintain_visible_position = next.maintainVisibleContentPosition.has_value();
	settings.min_index_for_visible = next.maintainVisibleContentPosition ? next.maintainVisibleContentPosition->minIndexForVisible : 0;
	scroll->set_settings(settings);
	scroll->set_overlay_indicators(next.horizontal && next.showsHorizontalScrollIndicator, !next.horizontal && next.showsVerticalScrollIndicator);
	if (!next.scrollEnabled) {
		scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
		scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	}
	scroll->set_follow_focus(true);
	const auto previous = std::static_pointer_cast<const ScrollViewProps>(p_old);
	if (!previous || previous->contentOffset != next.contentOffset) {
		scroll->scroll_to(Point2(next.contentOffset.x, next.contentOffset.y), false);
	}
}

void RNScrollComponentView::update_state(const State::Shared &p_state) {
	scroll->prepare_anchor();
	RNComponentView::update_state(p_state);
	const ScrollViewState &data = std::static_pointer_cast<const ConcreteState<ScrollViewState>>(p_state)->getData();
	const Size content = data.getContentSize();
	scroll->set_content_extent(Size2(content.width, content.height));
	scroll->restore_visible_anchor();
}

void RNScrollComponentView::_emit(const StringName &p_event) {
	auto emitter = std::static_pointer_cast<const ScrollViewEventEmitter>(event_emitter);
	if (!emitter) {
		return;
	}
	ScrollEvent event;
	event.contentOffset = { .x = Float(scroll->get_h_scroll()), .y = Float(scroll->get_v_scroll()) };
	const Size2 content = scroll->get_content()->get_size();
	event.contentSize = { .width = content.x, .height = content.y };
	const Size2 viewport = scroll->usable_viewport().size;
	event.containerSize = { .width = viewport.x, .height = viewport.y };
	event.zoomScale = 1;
	if (p_event == StringName("topScroll")) {
		if (auto concrete = std::static_pointer_cast<const ConcreteState<ScrollViewState>>(state)) {
			const ScrollViewState &data = concrete->getData();
			concrete->updateState(ScrollViewState(event.contentOffset, data.contentBoundingRect, data.scrollAwayPaddingTop));
		}
		emitter->onScroll(event);
	} else if (p_event == StringName("topScrollBeginDrag")) {
		emitter->onScrollBeginDrag(event);
	} else if (p_event == StringName("topScrollEndDrag")) {
		emitter->onScrollEndDrag(ScrollEndDragEvent(event));
	} else if (p_event == StringName("topMomentumScrollBegin")) {
		emitter->onMomentumScrollBegin(event);
	} else if (p_event == StringName("topMomentumScrollEnd")) {
		emitter->onMomentumScrollEnd(event);
	}
}

void RNScrollComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	RNScrollCallbacks callbacks;
	callbacks.event = [this](const StringName &p_event) { _emit(p_event); };
	scroll->set_callbacks(callbacks);
}

void RNScrollComponentView::handle_command(const std::string &p_name, const folly::dynamic &p_args) {
	if (p_name == "scrollTo" && p_args.size() == 3) {
		scroll->scroll_to(Point2(p_args[0].asDouble(), p_args[1].asDouble()), p_args[2].asBool());
	} else if (p_name == "scrollToEnd" && p_args.size() == 1) {
		scroll->scroll_to(scroll->get_content()->get_size(), p_args[0].asBool());
	} else if (p_name == "flashScrollIndicators") {
		scroll->get_h_scroll_bar()->queue_redraw();
		scroll->get_v_scroll_bar()->queue_redraw();
	}
}

bool RNScrollComponentView::owns_input_control(Control *p_control) const {
	return p_control == scroll || p_control == scroll->get_content() || p_control == scroll->get_h_scroll_bar() || p_control == scroll->get_v_scroll_bar();
}
