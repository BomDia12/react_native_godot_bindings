#pragma once

#include "../mounting/rn_component_view_registry.h"
#include "../turbomodules/rn_godot_turbo_module_provider.h"

#include <react/renderer/componentregistry/ComponentDescriptorProvider.h>

#include <map>
#include <string>
#include <vector>

// Native components and TurboModules that other Godot modules add to every
// runtime. Modules register them at MODULE_INITIALIZATION_LEVEL_SCENE, before
// any runtime starts.
class RNExtensionRegistry {
public:
	struct Component {
		facebook::react::ComponentDescriptorProvider descriptor;
		RNComponentViewFactory view;
	};

	static void add_component(facebook::react::ComponentDescriptorProvider p_descriptor, RNComponentViewFactory p_view);
	static void add_turbo_module(const std::string &p_name, facebook::react::RNTurboModuleFactory p_factory);

	static const std::vector<Component> &get_components();
	static const std::map<std::string, facebook::react::RNTurboModuleFactory> &get_turbo_modules();
};
