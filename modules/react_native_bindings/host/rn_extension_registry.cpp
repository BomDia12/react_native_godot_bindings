#include "rn_extension_registry.h"

using namespace facebook::react;

namespace {

std::vector<RNExtensionRegistry::Component> &components() {
	static std::vector<RNExtensionRegistry::Component> registered;
	return registered;
}

std::map<std::string, RNTurboModuleFactory> &turbo_modules() {
	static std::map<std::string, RNTurboModuleFactory> registered;
	return registered;
}

} // namespace

void RNExtensionRegistry::add_component(ComponentDescriptorProvider p_descriptor, RNComponentViewFactory p_view) {
	components().push_back({ std::move(p_descriptor), std::move(p_view) });
}

void RNExtensionRegistry::add_turbo_module(const std::string &p_name, RNTurboModuleFactory p_factory) {
	turbo_modules()[p_name] = std::move(p_factory);
}

const std::vector<RNExtensionRegistry::Component> &RNExtensionRegistry::get_components() {
	return components();
}

const std::map<std::string, RNTurboModuleFactory> &RNExtensionRegistry::get_turbo_modules() {
	return turbo_modules();
}
