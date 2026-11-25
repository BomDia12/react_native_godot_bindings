#include "rn_image_component_view.h"

#include "../host/rn_main_thread_queue.h"

#include <react/renderer/components/image/ImageEventEmitter.h>
#include <react/renderer/components/image/ImageProps.h>
#include <react/renderer/components/image/ImageState.h>
#include <react/renderer/core/ConcreteState.h>

using namespace facebook::react;

namespace {

TextureRect::StretchMode stretch_mode(ImageResizeMode p_mode) {
	switch (p_mode) {
		case ImageResizeMode::Cover:
			return TextureRect::STRETCH_KEEP_ASPECT_COVERED;
		case ImageResizeMode::Contain:
			return TextureRect::STRETCH_KEEP_ASPECT_CENTERED;
		case ImageResizeMode::Center:
		case ImageResizeMode::None:
			return TextureRect::STRETCH_KEEP_CENTERED;
		case ImageResizeMode::Repeat:
			return TextureRect::STRETCH_TILE;
		default:
			return TextureRect::STRETCH_SCALE;
	}
}

} // namespace

// Results reach the view only while it observes this observer's request.
class RNImageComponentView::Observer final : public ImageResponseObserver, public std::enable_shared_from_this<Observer> {
	std::shared_ptr<RNMainThreadQueue> main_thread;

	void _deliver(std::shared_ptr<const RNImageResource> p_resource, String p_error) const {
		if (!main_thread->is_main_thread()) {
			main_thread->post([self = shared_from_this(), p_resource, p_error] { self->_deliver(p_resource, p_error); });
			return;
		}
		if (view) {
			view->_loaded(p_resource, p_error);
		}
	}

public:
	// Read and cleared on the main thread.
	RNImageComponentView *view;

	Observer(RNImageComponentView *p_view, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
			main_thread(std::move(p_main_thread)), view(p_view) {}
	void didReceiveProgress(float, int64_t, int64_t) const override {}
	void didReceiveImage(const ImageResponse &p_response) const override {
		_deliver(std::static_pointer_cast<const RNImageResource>(p_response.getImage()), String());
	}
	void didReceiveFailure(const ImageLoadError &p_error) const override {
		const auto message = std::static_pointer_cast<const std::string>(p_error.getError());
		_deliver(nullptr, message ? String::utf8(message->c_str()) : String("Image failed to load"));
	}
};

RNImageComponentView::RNImageComponentView(Tag p_tag, std::shared_ptr<RNMainThreadQueue> p_main_thread) :
		RNComponentView(p_tag, memnew(RNImageControl)), image(Object::cast_to<RNImageControl>(get_control())), main_thread(std::move(p_main_thread)) {
}

RNImageComponentView::~RNImageComponentView() {
	_observe(nullptr);
}

void RNImageComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &image_props = static_cast<const ImageProps &>(*p_new);
	image->set_stretch_mode(stretch_mode(image_props.resizeMode));
	image->set_self_modulate(rn_godot_color(image_props.tintColor, ::Color(1, 1, 1)));
	_apply_corners();
}

void RNImageComponentView::update_state(const State::Shared &p_state) {
	RNComponentView::update_state(p_state);
	const ImageState &data = std::static_pointer_cast<const ConcreteState<ImageState>>(p_state)->getData();
	const std::shared_ptr<const ImageResponseObserverCoordinator> &request = data.getImageRequest().getSharedObserverCoordinator();
	if (request == coordinator) {
		return;
	}
	source = data.getImageSource();
	if (source.uri.empty()) {
		_observe(nullptr);
		image->set_resource(nullptr);
		return;
	}
	_observe(request);
}

void RNImageComponentView::_observe(const std::shared_ptr<const ImageResponseObserverCoordinator> &p_coordinator) {
	if (observer) {
		observer->view = nullptr;
		coordinator->removeObserver(observer);
		observer.reset();
	}
	coordinator = p_coordinator;
	if (!coordinator) {
		return;
	}
	observer = std::make_shared<Observer>(this, main_thread);
	if (auto emitter = std::static_pointer_cast<const ImageEventEmitter>(event_emitter)) {
		emitter->onLoadStart();
	}
	// A cached or local image completes inside addObserver, within this mount.
	coordinator->addObserver(observer);
}

// The texture's pixels are the asset's points times the scale of the source
// entry it was picked from; upstream overwrites the state source's scale.
float RNImageComponentView::_asset_scale() const {
	for (const ImageSource &entry : static_cast<const ImageProps &>(*props).sources) {
		if (entry.uri == source.uri && entry.scale > 0) {
			return entry.scale;
		}
	}
	return 1;
}

void RNImageComponentView::_loaded(const std::shared_ptr<const RNImageResource> &p_resource, const String &p_error) {
	const auto emitter = std::static_pointer_cast<const ImageEventEmitter>(event_emitter);
	if (p_resource) {
		image->set_resource(p_resource);
		if (emitter) {
			// As on iOS, the size is in points and the scale is the state's
			// point scale factor, which the event multiplies back in.
			const float scale = _asset_scale();
			ImageSource loaded = source;
			loaded.size = { .width = p_resource->texture->get_width() / scale, .height = p_resource->texture->get_height() / scale };
			emitter->onLoad(loaded);
		}
	} else if (emitter) {
		emitter->onError(ImageErrorInfo{ .error = p_error.utf8().get_data() });
	}
	if (emitter) {
		emitter->onLoadEnd();
	}
}

void RNImageComponentView::update_layout(const LayoutMetrics &p_metrics) {
	RNComponentView::update_layout(p_metrics);
	_apply_corners();
}

void RNImageComponentView::_apply_corners() {
	if (props) {
		const Rect2 rect(Point2(), Size2(layout_metrics.frame.size.width, layout_metrics.frame.size.height));
		image->set_corners(rn_rounded_rect(rect, props->resolveBorderMetrics(layout_metrics).borderRadii));
	}
}
