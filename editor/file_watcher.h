#pragma once

#include "editor/plugins/editor_plugin.h"

class FileWatcher: public EditorPlugin {
    GDCLASS(FileWatcher, EditorPlugin);

    String path;
    uint64_t last_modified_time = 0;

protected:
    static void _bind_methods();

public:
    FileWatcher();
    ~FileWatcher() override;

    void _enter_tree();
    void _exit_tree();
    void _process(double delta);

private:
    void _ensure_file_exists();
    void _on_filesystem_changed();
}
