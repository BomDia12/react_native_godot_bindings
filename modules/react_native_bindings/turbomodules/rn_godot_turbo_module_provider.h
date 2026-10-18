#pragma once

#include "rn_application_modules.h"
#include "rn_godot_scene_module.h"
#include "rn_host_module_context.h"
#include "rn_network_modules.h"

#include <react/io/IImageLoader.h>
#include <react/nativemodule/TurboModuleProvider.h>

#include <map>

namespace facebook::react {

using RNTurboModuleFactory = std::function<std::shared_ptr<TurboModule>(const std::shared_ptr<CallInvoker> &, const RNHostModuleContext &)>;

struct RNTurboModuleContext {
	RNHostModuleContext host;
	RNApplicationModuleContext application;
	RNNetworkModuleContext network;
	std::shared_ptr<RNSceneBindings> scenes;
	std::shared_ptr<IImageLoader> image_loader;
	std::map<std::string, RNTurboModuleFactory> extensions;
	// The Metro URL of a bundle served by the developer server, which JS uses
	// to find Metro for symbolication and Fast Refresh.
	std::string source_url;
};

// Godot's modules take precedence over the ReactCxxPlatform defaults that ReactHost falls back to.
TurboModuleProvider rn_godot_turbo_module_provider(RNTurboModuleContext context);

} // namespace facebook::react
