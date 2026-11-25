#pragma once

#include "../native_modules/rn_image_service.h"

#include <react/renderer/imagemanager/primitives.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// The textures of a control with several image props, each loaded through the
// image service and replaced as its source changes. It belongs to the main thread.
class RNTextureSources {
public:
	using Applied = std::function<void(int p_slot, const Ref<Texture2D> &p_texture)>;
	using Failed = std::function<void(int p_slot, const String &p_message)>;

private:
	struct Slot {
		std::string uri;
		uint64_t subscription = 0;
		uint64_t token = 0;
		bool pending = false;
		std::shared_ptr<const RNImageResource> resource;
	};
	struct Shared {
		std::vector<Slot> slots;
		Applied applied;
		Failed failed;
	};
	std::shared_ptr<RNImageService> service;
	std::shared_ptr<Shared> shared;

public:
	RNTextureSources(std::shared_ptr<RNImageService> p_service, int p_slots, Applied p_applied, Failed p_failed);
	~RNTextureSources();
	RNTextureSources(const RNTextureSources &) = delete;
	RNTextureSources &operator=(const RNTextureSources &) = delete;

	void set_source(int p_slot, const facebook::react::ImageSource &p_source);
	void clear_callbacks();
};
