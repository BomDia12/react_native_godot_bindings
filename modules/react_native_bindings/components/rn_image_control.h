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
struct RNImageCallbacks {
	std::function<void()> load_start;
	std::function<void(const RNImageSource &, const Size2 &)> load;
	std::function<void(const String &)> error;
	std::function<void()> load_end;
	std::function<void()> source_changed;
};
class RNImageControl : public TextureRect {
	GDCLASS(RNImageControl, TextureRect);
	std::shared_ptr<RNImageService> service;
	std::shared_ptr<const RNImageResource> resource;
	RNImageCallbacks callbacks;
	String source_key;
	uint64_t source_token = 0;
	uint64_t subscription = 0;
	Ref<Texture2D> observed_texture;
	void _source_changed();
	void _observe_texture(const Ref<Texture2D> &p_texture);
	void _loaded(uint64_t p_token, const RNImageSource &p_source, const RNImageResult &p_result);

protected:
	static void _bind_methods() {}

public:
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	RNImageControl();
	~RNImageControl();
	void set_callbacks(RNImageCallbacks p_callbacks) { callbacks = std::move(p_callbacks); }
	void load(const RNImageSource *p_source, const std::shared_ptr<RNImageService> &p_service);
	void publish(const RNPreparedHostState &p_state, const RNHostContext &p_context);
};
std::shared_ptr<const RNHostDescriptor> rn_image_descriptor();
