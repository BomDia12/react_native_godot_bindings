#include "register_types.h"

#include "components/rn_builtin_descriptors.h"
#include "components/rn_image_control.h"
#include "components/rn_presentation_control.h"
#include "components/rn_scroll_control.h"
#include "components/rn_small_controls.h"
#include "components/rn_text_control.h"
#include "components/rn_text_input_control.h"
#include "components/rn_view_control.h"
#include "examples/rn_example_meter.h"
#include "examples/rn_example_scene_module.h"
#include "fabric/rn_shadow_node.h"
#include "interop/rn_scene_binding.h"
#include "native_modules/rn_alert_service.h"
#include "native_modules/rn_application_services.h"
#include "native_modules/rn_blob_service.h"
#include "native_modules/rn_builtin_native_modules.h"
#include "native_modules/rn_godot_scene_module.h"
#include "native_modules/rn_http_service.h"
#include "native_modules/rn_websocket_module.h"
#include "root_view/react_native_root_view.h"
#include "runtime/react_native_runtime_coordinator.h"
#include "runtime/rn_service_settings.h"
#include "singletons/hermes_runtime_singleton.h"
#include "singletons/react_native_file_singleton.h"

#include "core/config/project_settings.h"

#ifdef TOOLS_ENABLED
#include "editor/react_native_file_editor_plugin.h"
#endif

#include "core/config/engine.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"

#ifdef TESTS_ENABLED
void rn_force_link_native_module_registry_tests();
#endif

static GodotAlerts *godot_alerts = nullptr;
static ReactNativeFileSingleton *react_native_file_singleton = nullptr;
static HermesRuntimeSingleton *hermes_runtime_singleton = nullptr;
static ReactNativeRuntimeCoordinator *react_native_runtime_coordinator = nullptr;

void initialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
#ifdef TESTS_ENABLED
		rn_force_link_native_module_registry_tests();
#endif
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
		GLOBAL_DEF("react_native/text/font_aliases", Dictionary());
		rn_register_image_settings();
		rn_register_service_settings();
		ClassDB::register_class<GodotAlerts>();
		godot_alerts = memnew(GodotAlerts);
		godot_alerts->configure(react_native_runtime_coordinator->get_state());
		Engine::get_singleton()->add_singleton(Engine::Singleton("GodotAlerts", godot_alerts, "GodotAlerts"));
		ClassDB::register_abstract_class<RNHTTPService>();
		ClassDB::register_abstract_class<RNTextNativeState>();
		ClassDB::register_class<RNTextControl>();
		ClassDB::register_class<RNViewControl>();
		ClassDB::register_class<RNImageControl>();
		ClassDB::register_class<RNModalControl>();
		ClassDB::register_class<RNWindowControl>();
		ClassDB::register_class<RNSwitchControl>();
		ClassDB::register_class<RNActivityIndicatorControl>();
		ClassDB::register_class<RNScrollControl>();
		ClassDB::register_abstract_class<RNTextInputState>();
		ClassDB::register_class<RNTextInputControl>();
		RNError registration_error;
		std::shared_ptr<RNHostDescriptorRegistry> descriptors = react_native_runtime_coordinator->get_descriptor_registry();
		ERR_FAIL_COND_MSG(!descriptors || !rn_register_builtin_descriptors(*descriptors, registration_error) || !rn_register_example_meter(*descriptors, registration_error), registration_error.describe());
		descriptors->freeze();
		std::shared_ptr<RNNativeModuleRegistry> modules = react_native_runtime_coordinator->get_native_module_registry();
		ERR_FAIL_COND_MSG(!modules || !rn_register_builtin_native_modules(*modules, registration_error) || !rn_register_example_scene_module(*modules, registration_error), registration_error.describe());
		ERR_FAIL_COND_MSG(!rn_register_godot_scene_module(*modules, registration_error), registration_error.describe());
		ERR_FAIL_COND_MSG(!rn_register_application_services(*modules, react_native_runtime_coordinator->get_state(), registration_error), registration_error.describe());
		auto state = react_native_runtime_coordinator->get_state();
		ERR_FAIL_COND_MSG(!rn_register_blob_module(*modules, [state] { return state->blobs; }, registration_error), registration_error.describe());
		ERR_FAIL_COND_MSG(!rn_register_http_module(*modules, [state] { return ObjectDB::get_instance(state->http_id) ? state->http : nullptr; }, [state] { return state->blobs; }, registration_error), registration_error.describe());
		ERR_FAIL_COND_MSG(!rn_register_websocket_module(*modules, [state] { return state->service_settings; }, [state] { return state->blobs; }, registration_error), registration_error.describe());
		ERR_FAIL_COND_MSG(!rn_register_alert_module(*modules, registration_error), registration_error.describe());
		modules->freeze_definitions();
		ClassDB::register_class<RNSceneBinding>();
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
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && react_native_runtime_coordinator) {
		react_native_runtime_coordinator->shutdown_scene();
		Engine::get_singleton()->remove_singleton("GodotAlerts");
		memdelete(godot_alerts);
		godot_alerts = nullptr;
	}

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
