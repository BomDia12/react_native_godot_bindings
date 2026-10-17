#include "rn_image_component_view.h"

#include <react/renderer/components/image/ImageEventEmitter.h>
#include <react/renderer/components/image/ImageProps.h>

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

RNImageComponentView::RNImageComponentView(Tag p_tag, std::shared_ptr<RNImageService> p_service) :
		RNComponentView(p_tag, memnew(RNImageControl)), image(Object::cast_to<RNImageControl>(get_control())), service(std::move(p_service)) {
}

void RNImageComponentView::update_props(const Props::Shared &p_old, const Props::Shared &p_new) {
	RNComponentView::update_props(p_old, p_new);
	const auto &image_props = static_cast<const ImageProps &>(*p_new);
	image->set_stretch_mode(stretch_mode(image_props.resizeMode));
	image->set_self_modulate(rn_godot_color(image_props.tintColor, ::Color(1, 1, 1)));
	if (image_props.sources.empty() || image_props.sources[0].uri.empty()) {
		image->load(nullptr, service);
		return;
	}
	const ImageSource &upstream = image_props.sources[0];
	Dictionary source;
	source["uri"] = String::utf8(upstream.uri.c_str());
	source["scale"] = upstream.scale;
	Dictionary headers;
	for (const auto &[name, value] : upstream.headers) {
		headers[String::utf8(name.c_str())] = String::utf8(value.c_str());
	}
	source["headers"] = headers;
	RNImageSource normalized;
	RNError error;
	if (!RNImageService::normalize(source, normalized, error)) {
		image->load(nullptr, service);
		if (auto emitter = std::static_pointer_cast<const ImageEventEmitter>(event_emitter)) {
			emitter->onError(ImageErrorInfo{ .error = error.message.utf8().get_data() });
		}
		return;
	}
	image->load(&normalized, service);
}

void RNImageComponentView::update_event_emitter(const EventEmitter::Shared &p_emitter) {
	RNComponentView::update_event_emitter(p_emitter);
	auto emitter = std::static_pointer_cast<const ImageEventEmitter>(p_emitter);
	if (!emitter) {
		image->set_callbacks({});
		return;
	}
	RNImageCallbacks callbacks;
	callbacks.load_start = [emitter]() { emitter->onLoadStart(); };
	callbacks.load = [emitter](const RNImageSource &p_source, const Size2 &p_size) {
		ImageSource source;
		source.type = ImageSource::Type::Local;
		source.uri = p_source.uri.utf8().get_data();
		source.scale = p_source.scale;
		source.size = { .width = p_size.x, .height = p_size.y };
		emitter->onLoad(source);
	};
	callbacks.error = [emitter](const String &p_message) { emitter->onError(ImageErrorInfo{ .error = p_message.utf8().get_data() }); };
	callbacks.load_end = [emitter]() { emitter->onLoadEnd(); };
	image->set_callbacks(callbacks);
}
