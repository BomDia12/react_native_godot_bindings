#include "register_types.h"

#include "react_native_bindings/host/rn_extension_registry.h"
#include "rn_example_meter_component_view.h"
#include "rn_example_scene_module.h"

#include "core/object/class_db.h"

#include <react/renderer/components/RNGodotExampleSpec/ComponentDescriptors.h>

using namespace facebook::react;

void initialize_rn_godot_example_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_internal_class<RNExampleMeterControl>();
	ClassDB::register_class<RNExampleCounter>();
	RNExtensionRegistry::add_component(concreteComponentDescriptorProvider<RNExampleMeterComponentDescriptor>(), [](const ShadowView &p_view) {
		return std::make_unique<RNExampleMeterComponentView>(p_view.tag);
	});
	RNExtensionRegistry::add_turbo_module(std::string(RNExampleSceneModule::kModuleName), [](const std::shared_ptr<CallInvoker> &p_invoker, const RNHostModuleContext &p_host) {
		return std::make_shared<RNExampleSceneModule>(p_invoker, p_host);
	});
}

void uninitialize_rn_godot_example_module(ModuleInitializationLevel p_level) {
}
