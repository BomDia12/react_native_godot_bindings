#include "register_types.h"

#include "core/object/class_db.h"
#include "base_node/base_node.h"
#include "editor/file_watcher.h"

static FileWatcher * file_watcher = nullptr;

void initialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<BaseNode>();
        return;
    }

    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        file_watcher = memnew(FileWatcher);
        #ifdef TOOLS_ENABLED
            EditorPlugins::add_by_type<FileWatcher>(file_watcher);
        #endif
        return;
    }
}

void uninitialize_react_native_bindings_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && file_watcher) {
        memdelete(file_watcher);
        file_watcher = nullptr;
    }
}
