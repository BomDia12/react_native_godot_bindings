#include "register_types.h"

#include "base_node/base_node.h"
#include "singletons/react_native_file_singleton.h"

#include "core/config/engine.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"

static ReactNativeFileSingleton *react_native_file_singleton = nullptr;

void initialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<BaseNode>();
        ClassDB::register_class<ReactNativeFileSingleton>();

        ERR_FAIL_COND(react_native_file_singleton != nullptr);
        react_native_file_singleton = memnew(ReactNativeFileSingleton);
        Engine::get_singleton()->add_singleton(Engine::Singleton("ReactNativeFileSingleton", ReactNativeFileSingleton::get_singleton(), "ReactNativeFileSingleton"));

        return;
    }
    
    return;
}

void uninitialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        if (react_native_file_singleton) {
            Engine::get_singleton()->remove_singleton("ReactNativeFileSingleton");
            memdelete(react_native_file_singleton);
            react_native_file_singleton = nullptr;
        }
    }
}
