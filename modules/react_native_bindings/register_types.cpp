#include "register_types.h"

#include "components/rn_image_control.h"
#include "components/rn_presentation_control.h"
#include "components/rn_scroll_control.h"
#include "components/rn_small_controls.h"
#include "components/rn_text_control.h"
#include "components/rn_text_input_control.h"
#include "components/rn_view_control.h"
#include "examples/rn_example_meter.h"
#include "examples/rn_example_scene_module.h"
#include "host/react_native_host.h"
#include "host/rn_feature_flags.h"
#include "host/rn_logging.h"
#include "host/rn_service_settings.h"
#include "interop/rn_scene_binding.h"
#include "native_modules/rn_http_service.h"
#include "native_modules/rn_image_service.h"
#include "root_view/react_native_root_view.h"
#include "singletons/react_native_file_singleton.h"
#include "turbomodules/rn_godot_alerts.h"

#ifdef TOOLS_ENABLED
#include "editor/react_native_file_editor_plugin.h"
#endif

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/class_db.h"

#ifdef TESTS_ENABLED
void rn_force_link_tests();
#endif

static ReactNativeFileSingleton *react_native_file_singleton = nullptr;
static GodotAlerts *godot_alerts = nullptr;
static ReactNativeHost *react_native_host = nullptr;

void initialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
#ifdef TESTS_ENABLED
		rn_force_link_tests();
#endif
		// Upstream fixes its feature flags at first use, so they are installed
		// before anything, tests included, can touch React Native.
		rn_install_logging();
		rn_install_feature_flags();
		ClassDB::register_class<ReactNativeFileSingleton>();
		react_native_file_singleton = memnew(ReactNativeFileSingleton);
		Engine::get_singleton()->add_singleton(Engine::Singleton("ReactNativeFileSingleton", react_native_file_singleton, "ReactNativeFileSingleton"));
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GLOBAL_DEF("react_native/text/font_aliases", Dictionary());
		rn_register_image_settings();
		rn_register_service_settings();
		ClassDB::register_class<GodotAlerts>();
		godot_alerts = memnew(GodotAlerts);
		Engine::get_singleton()->add_singleton(Engine::Singleton("GodotAlerts", godot_alerts, "GodotAlerts"));
		ClassDB::register_abstract_class<RNHTTPService>();
		ClassDB::register_internal_class<RNHostLifecycle>();
		ClassDB::register_abstract_class<RNEvaluation>();
		ClassDB::register_class<ReactNativeHost>();
		react_native_host = memnew(ReactNativeHost);
		Engine::get_singleton()->add_singleton(Engine::Singleton("ReactNativeHost", react_native_host, "ReactNativeHost"));
		ClassDB::register_class<RNTextControl>();
		ClassDB::register_class<RNViewControl>();
		ClassDB::register_class<RNImageControl>();
		ClassDB::register_class<RNModalControl>();
		ClassDB::register_class<RNWindowControl>();
		ClassDB::register_class<RNSwitchControl>();
		ClassDB::register_class<RNActivityIndicatorControl>();
		ClassDB::register_class<RNScrollControl>();
		ClassDB::register_internal_class<RNScrollContentControl>();
		ClassDB::register_internal_class<RNScrollIndicators>();
		ClassDB::register_internal_class<RNModalOverlay>();
		ClassDB::register_internal_class<RNWindowContent>();
		ClassDB::register_internal_class<RNLineEditor>();
		ClassDB::register_internal_class<RNTextEditor>();
		ClassDB::register_internal_class<RNExampleMeterControl>();
		ClassDB::register_class<RNTextInputControl>();
		ClassDB::register_class<RNSceneBinding>();
		ClassDB::register_class<ReactNativeRootView>();
		ClassDB::register_class<RNExampleCounter>();
		return;
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<ReactNativeFileEditorPlugin>();
	}
#endif
}

void uninitialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (react_native_host) {
			Engine::get_singleton()->remove_singleton("ReactNativeHost");
			memdelete(react_native_host);
			react_native_host = nullptr;
		}
		if (godot_alerts) {
			Engine::get_singleton()->remove_singleton("GodotAlerts");
			memdelete(godot_alerts);
			godot_alerts = nullptr;
		}
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE && react_native_file_singleton) {
		Engine::get_singleton()->remove_singleton("ReactNativeFileSingleton");
		memdelete(react_native_file_singleton);
		react_native_file_singleton = nullptr;
	}
}
