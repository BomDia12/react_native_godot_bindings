#include "register_types.h"

#include "core/object/class_db.h"
#include "base_node/base_node.h"

void initialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Register your custom types here
    ClassDB::register_class<BaseNode>();
}

void uninitialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    // Unregister your custom types here if needed
}
