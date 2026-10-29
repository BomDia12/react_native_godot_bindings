#pragma once

#include "rn_application_modules.h"
#include "rn_godot_host_module.h"
#include "rn_godot_scene_module.h"
#include "rn_host_module_context.h"
#include "rn_network_modules.h"

#include <react/io/IImageLoader.h>
#include <react/nativemodule/TurboModuleProvider.h>

#include <map>
#include <mutex>
#include <string>

namespace facebook::react {

// Where the bundle was loaded from: its Metro URL, which JS uses to find Metro
// for symbolication, Fast Refresh and assets, or its file, next to which assets
// are staged. The host sets it once it knows which of the two it loads, before
// JS can create the SourceCode module that reads it.
class RNSourceURL {
	mutable std::mutex mutex;
	std::string url;

public:
	void set(std::string p_url) {
		std::lock_guard lock(mutex);
		url = std::move(p_url);
	}
	std::string get() const {
		std::lock_guard lock(mutex);
		return url;
	}
};

using RNTurboModuleFactory = std::function<std::shared_ptr<TurboModule>(const std::shared_ptr<CallInvoker> &, const RNHostModuleContext &)>;

struct RNTurboModuleContext {
	RNHostModuleContext host;
	RNApplicationModuleContext application;
	RNNetworkModuleContext network;
	std::shared_ptr<RNSceneBindings> scenes;
	std::shared_ptr<RNGodotHostEvents> host_events;
	std::shared_ptr<IImageLoader> image_loader;
	std::map<std::string, RNTurboModuleFactory> extensions;
	std::shared_ptr<RNSourceURL> source_url = std::make_shared<RNSourceURL>();
	bool fast_refresh = true;
};

// Godot's modules take precedence over the ReactCxxPlatform defaults that ReactHost falls back to.
TurboModuleProvider rn_godot_turbo_module_provider(RNTurboModuleContext context);

} // namespace facebook::react
