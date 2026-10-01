#include "register_types.h"

#include "components/rn_builtin_descriptors.h"
#include "examples/rn_example_meter.h"
#include "examples/rn_example_scene_module.h"
#include "fabric/rn_shadow_node.h"
#include "native_modules/rn_builtin_native_modules.h"
#include "root_view/react_native_root_view.h"
#include "runtime/react_native_runtime_coordinator.h"
#include "singletons/hermes_runtime_singleton.h"
#include "singletons/react_native_file_singleton.h"

#ifdef TOOLS_ENABLED
#include "editor/react_native_file_editor_plugin.h"
#endif

#include "core/config/engine.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"

static ReactNativeFileSingleton *react_native_file_singleton = nullptr;
static HermesRuntimeSingleton *hermes_runtime_singleton = nullptr;
static ReactNativeRuntimeCoordinator *react_native_runtime_coordinator = nullptr;

void initialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		ClassDB::register_class<HermesRuntimeSingleton>();
		ClassDB::register_class<ReactNativeFileSingleton>();

		ERR_FAIL_COND(hermes_runtime_singleton != nullptr);
		hermes_runtime_singleton = memnew(HermesRuntimeSingleton);
		Engine::get_singleton()->add_singleton(Engine::Singleton("HermesRuntime", HermesRuntimeSingleton::get_singleton(), "HermesRuntimeSingleton"));

		ERR_FAIL_COND(react_native_file_singleton != nullptr);
		react_native_file_singleton = memnew(ReactNativeFileSingleton);
		Engine::get_singleton()->add_singleton(Engine::Singleton("ReactNativeFileSingleton", ReactNativeFileSingleton::get_singleton(), "ReactNativeFileSingleton"));

		ERR_FAIL_COND(react_native_runtime_coordinator != nullptr);
		react_native_runtime_coordinator = memnew(ReactNativeRuntimeCoordinator);
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		RNError registration_error;
		std::shared_ptr<RNHostDescriptorRegistry> descriptors = react_native_runtime_coordinator->get_descriptor_registry();
		ERR_FAIL_COND_MSG(!descriptors || !rn_register_builtin_descriptors(*descriptors, registration_error) || !rn_register_example_meter(*descriptors, registration_error), registration_error.describe());
		descriptors->freeze();
		std::shared_ptr<RNNativeModuleRegistry> modules = react_native_runtime_coordinator->get_native_module_registry();
		ERR_FAIL_COND_MSG(!modules || !rn_register_builtin_native_modules(*modules, registration_error) || !rn_register_example_scene_module(*modules, registration_error), registration_error.describe());
		modules->freeze_definitions();
		ClassDB::register_class<ReactNativeRootView>();
		ClassDB::register_class<RNExampleCounter>();

		// Not meant to be instantiated from script: registered so a Ref<RNShadowNode>
		// survives the Variant round-trip that call_deferred("mount", ...) does.
		ClassDB::register_abstract_class<RNShadowNode>();

		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<ReactNativeFileEditorPlugin>();
		return;
	}
#endif

	return;
}

void uninitialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		if (react_native_runtime_coordinator) {
			memdelete(react_native_runtime_coordinator);
			react_native_runtime_coordinator = nullptr;
		}

		if (react_native_file_singleton) {
			Engine::get_singleton()->remove_singleton("ReactNativeFileSingleton");
			memdelete(react_native_file_singleton);
			react_native_file_singleton = nullptr;
		}

		if (hermes_runtime_singleton) {
			Engine::get_singleton()->remove_singleton("HermesRuntime");
			memdelete(hermes_runtime_singleton);
			hermes_runtime_singleton = nullptr;
		}
	}
}
