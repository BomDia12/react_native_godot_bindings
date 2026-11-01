#include "rn_texture_sources.h"

using namespace facebook::react;

RNTextureSources::RNTextureSources(std::shared_ptr<RNImageService> p_service, int p_slots, Applied p_applied, Failed p_failed) :
		service(std::move(p_service)), shared(std::make_shared<Shared>()) {
	shared->slots.resize(p_slots);
	shared->applied = std::move(p_applied);
	shared->failed = std::move(p_failed);
}

RNTextureSources::~RNTextureSources() {
	clear_callbacks();
	for (const Slot &slot : shared->slots) {
		if (slot.subscription) {
			service->cancel(slot.subscription);
		}
	}
}

void RNTextureSources::clear_callbacks() {
	shared->applied = nullptr;
	shared->failed = nullptr;
}

void RNTextureSources::set_source(int p_slot, const ImageSource &p_source) {
	Slot &slot = shared->slots[p_slot];
	if (slot.uri == p_source.uri) {
		return;
	}
	slot.uri = p_source.uri;
	const uint64_t token = ++slot.token;
	if (slot.subscription) {
		service->cancel(slot.subscription);
		slot.subscription = 0;
	}
	slot.resource.reset();
	if (p_source.uri.empty()) {
		if (shared->applied) {
			shared->applied(p_slot, Ref<Texture2D>());
		}
		return;
	}
	Dictionary source;
	source["uri"] = String::utf8(p_source.uri.c_str());
	source["scale"] = p_source.scale;
	RNImageSource normalized;
	RNError error;
	if (!RNImageService::normalize(source, normalized, error)) {
		if (shared->failed) {
			shared->failed(p_slot, error.message);
		}
		return;
	}
	slot.pending = true;
	const std::weak_ptr<Shared> weak = shared;
	const uint64_t subscription = service->request(normalized, [weak, p_slot, token](RNImageResult p_result) {
		const std::shared_ptr<Shared> state = weak.lock();
		if (!state || state->slots[p_slot].token != token) {
			return;
		}
		Slot &loaded = state->slots[p_slot];
		loaded.subscription = 0;
		loaded.pending = false;
		if (p_result.error.is_set()) {
			if (state->failed) {
				state->failed(p_slot, p_result.error.message);
			}
			return;
		}
		loaded.resource = p_result.resource;
		if (state->applied) {
			state->applied(p_slot, p_result.resource->texture);
		}
	});
	if (slot.token == token && slot.pending) {
		slot.subscription = subscription;
	}
}
