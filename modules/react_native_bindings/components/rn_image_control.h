#pragma once
#include "../native_modules/rn_image_service.h"
#include "rn_host_descriptor.h"

#include "scene/gui/texture_rect.h"
struct RNImageData : RNComponentData {
	RNImageSource source;
	bool has_source = false;
	Size2 intrinsic;
	std::shared_ptr<RNImageService> service;
	std::shared_ptr<const RNImageResource> resource;
};
class RNImageControl : public TextureRect {
	GDCLASS(RNImageControl, TextureRect);
	std::shared_ptr<RNImageService> service;
	std::shared_ptr<const RNImageResource> resource;
	RNHostContext published_context;
	String source_key;
	uint64_t source_token = 0;
	uint64_t subscription = 0;
	Ref<Texture2D> observed_texture;
	void _source_changed();
	void _observe_texture(const Ref<Texture2D> &p_texture);
	void _loaded(uint64_t p_token, const RNImageSource &p_source, const RNImageResult &p_result);
	void _emit(const StringName &p_name, const Dictionary &p_payload = Dictionary());

protected:
	static void _bind_methods() {}

public:
	RNImageControl();
	~RNImageControl();
	void publish(const RNPreparedHostState &p_state, const RNHostContext &p_context);
};
std::shared_ptr<const RNHostDescriptor> rn_image_descriptor();
