#pragma once

#include "rn_application_modules.h"
#include "rn_network_modules.h"

#include <react/io/IImageLoader.h>
#include <react/nativemodule/TurboModuleProvider.h>

#include <map>

namespace facebook::react {

using RNTurboModuleFactory = std::function<std::shared_ptr<TurboModule>(const std::shared_ptr<CallInvoker> &)>;

struct RNTurboModuleContext {
	RNApplicationModuleContext application;
	RNNetworkModuleContext network;
	std::shared_ptr<IImageLoader> image_loader;
	std::map<std::string, RNTurboModuleFactory> extensions;
};

// Godot's modules take precedence over the ReactCxxPlatform defaults that ReactHost falls back to.
TurboModuleProvider rn_godot_turbo_module_provider(RNTurboModuleContext context);

} // namespace facebook::react
