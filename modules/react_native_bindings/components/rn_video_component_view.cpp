#include "rn_video_component_view.h"

#include "rn_resource_requests.h"

#include "core/object/callable_mp.h"

#include <react/renderer/components/ReactNativeGodotSpec/EventEmitters.h>
#include <react/renderer/components/ReactNativeGodotSpec/Props.h>

using namespace facebook::react;

RNVideoControl::RNVideoControl() {
	connect(SceneStringName(finished), callable_mp(this, &RNVideoControl::_finished));
}

void RNVideoControl::_finished() {
	if (ended) {
		ended();
	}
}

RNVideoComponentView::RNVideoComponentView(Tag p_tag) :
		RNComponentView(p_tag, memnew(RNVideoControl)), video(Object::cast_to<RNVideoControl>(get_control())) {
}

RNVideoComponentView::~RNVideoComponentView() {
	RNResourceRequests::cancel(request);
	if (has_control()) {
		video->set_ended_callback(nullptr);
	}
}

void RNVideoComponentView::_load(const String &p_source) {
	if (p_source == source) {
		return;
	}
	source = p_source;
	RNResourceRequests::cancel(request);
	request = 0;
	video->stop();
	video->set_stream(Ref<VideoStream>());
	if (!source.is_empty()) {
		request = RNResourceRequests::request(source, "VideoStream", [this](const Ref<Resource> &p_resource, const String &p_error) { _loaded(p_resource, p_error); });
	}
}

void RNVideoComponentView::_loaded(const Ref<Resource> &p_resource, const String &p_error) {
	request = 0;
	const Ref<VideoStream> stream = p_resource;
	if (stream.is_null()) {
		failure_pending = p_error.is_empty() ? vformat("'%s' is not a VideoStream.", source) : p_error;
		ERR_PRINT(vformat("E_RESOURCE: GodotVideo on React tag %d: %s", get_tag(), failure_pending));
		_report();
		return;
	}
	video->set_stream(stream);
	if (static_cast<const GodotVideoProps &>(*props).autoplay) {
		video->set_autoplay(true);
		if (video->is_inside_tree()) {
			video->play();
		}
	}
	load_pending = true;
	_report();
}

// A cached stream loads while props apply, before the view has an event emitter.
void RNVideoComponentView::_report() {
	const auto emitter = std::static_pointer_cast<const GodotVideoEventEmitter>(event_emitter);
	if (!emitter) {
		return;
	}
	if (load_pending) {
		load_pending = false;
		emitter->onLoad({});
	}
	if (!failure_pending.is_empty()) {
		emitter->onError({ .message = failure_pending.utf8().get_data() });
		failure_pending = String();
	}
}

void RNVideoComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &next = static_cast<const GodotVideoProps &>(*p_new);
	video->set_expand(next.expand);
	video->set_loop(next.loop);
	video->set_volume(next.volume);
	video->set_bus(String::utf8(next.bus.c_str()));
	video->set_paused(next.paused);
	_load(String::utf8(next.source.c_str()));
}

void RNVideoComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const GodotVideoEventEmitter>(p_emitter);
	video->set_ended_callback(emitter ? std::function<void()>([emitter]() { emitter->onEnd({}); }) : nullptr);
	_report();
}

void RNVideoComponentView::handle_command(const std::string &p_name, const folly::dynamic &p_args) {
	if (p_name == "play") {
		video->play();
	} else if (p_name == "seek" && p_args.size() == 1) {
		video->set_stream_position(p_args[0].asDouble());
	}
}
