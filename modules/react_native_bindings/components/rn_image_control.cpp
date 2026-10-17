#include "rn_image_control.h"

#include "core/object/callable_mp.h"

RNImageControl::RNImageControl() {
	set_expand_mode(EXPAND_IGNORE_SIZE);
	set_clip_contents(true);
}
RNImageControl::~RNImageControl() {
	_observe_texture(Ref<Texture2D>());
	++source_token;
	if (service && subscription) {
		service->cancel(subscription);
	}
}
void RNImageControl::load(const RNImageSource *p_source, const std::shared_ptr<RNImageService> &p_service) {
	const String key = p_source ? p_source->key : String();
	if (key == source_key && service == p_service) {
		return;
	}
	++source_token;
	if (service && subscription) {
		service->cancel(subscription);
	}
	subscription = 0;
	source_key = key;
	service = p_service;
	if (!p_source) {
		_observe_texture(Ref<Texture2D>());
		resource.reset();
		set_texture(Ref<Texture2D>());
		return;
	}
	if (callbacks.load_start) {
		callbacks.load_start();
	}
	const uint64_t token = source_token;
	const ObjectID host_id = get_instance_id();
	const auto request = service->request(*p_source, [host_id, token, source = *p_source](RNImageResult result) { auto *host = Object::cast_to<RNImageControl>(ObjectDB::get_instance(host_id)); if (host) { host->_loaded(token, source, result); } });
	if (token == source_token) {
		subscription = request;
	}
}
void RNImageControl::_loaded(uint64_t p_token, const RNImageSource &p_source, const RNImageResult &p_result) {
	if (p_token != source_token) {
		return;
	}
	subscription = 0;
	if (p_result.error.is_set()) {
		if (callbacks.error) {
			callbacks.error(p_result.error.message);
		}
	} else {
		resource = p_result.resource;
		set_texture(resource->texture);
		_observe_texture(resource->texture);
		if (callbacks.load) {
			callbacks.load(p_source, Size2(resource->texture->get_size()) / p_source.scale);
		}
	}
	if (callbacks.load_end) {
		callbacks.load_end();
	}
}
void RNImageControl::_observe_texture(const Ref<Texture2D> &p_texture) {
	if (observed_texture == p_texture) {
		return;
	}
	if (observed_texture.is_valid()) {
		observed_texture->disconnect("changed", callable_mp(this, &RNImageControl::_source_changed));
	}
	observed_texture = p_texture;
	if (observed_texture.is_valid()) {
		observed_texture->connect("changed", callable_mp(this, &RNImageControl::_source_changed));
	}
}
void RNImageControl::_source_changed() {
	if (callbacks.source_changed) {
		callbacks.source_changed();
	}
}
