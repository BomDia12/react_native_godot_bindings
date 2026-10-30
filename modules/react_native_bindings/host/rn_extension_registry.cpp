#include "rn_extension_registry.h"

#include "core/error/error_macros.h"
#include "core/variant/variant.h"

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

// Two libraries registering the same name would replace each other, so the
// first one keeps it.
void RNExtensionRegistry::add_component(ComponentDescriptorProvider p_descriptor, RNComponentViewFactory p_view) {
	for (const Component &component : components()) {
		ERR_FAIL_COND_MSG(std::string(component.descriptor.name) == p_descriptor.name, vformat("The native component '%s' is already registered.", p_descriptor.name));
	}
	components().push_back({ std::move(p_descriptor), std::move(p_view) });
}

void RNExtensionRegistry::add_turbo_module(const std::string &p_name, RNTurboModuleFactory p_factory) {
	ERR_FAIL_COND_MSG(turbo_modules().contains(p_name), vformat("The TurboModule '%s' is already registered.", p_name.c_str()));
	turbo_modules()[p_name] = std::move(p_factory);
}

const std::vector<RNExtensionRegistry::Component> &RNExtensionRegistry::get_components() {
	return components();
}

const std::map<std::string, RNTurboModuleFactory> &RNExtensionRegistry::get_turbo_modules() {
	return turbo_modules();
}
