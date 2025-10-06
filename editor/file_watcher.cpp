#include "file_watcher.h"

#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

FileWatcher::FileWatcher() {
    file_path = "res://react_native/index.txt";
}

FileWatcher::~FileWatcher() {
}

void FileWatcher::_enter_tree() {
    _ensure_file_exists();
    last_modified_time = DirAccess::get_modified_time(file_path);

    EditorFileSystem::get_singleton()->connect(
        "filesystem_changed",
        callable_mp(this, &FileWatcher::_on_filesystem_changed));

    set_process(true);
}

void FileWatcher::_exit_tree() {
    EditorFileSystem::get_singleton()->disconnect(
        "filesystem_changed",
        callable_mp(this, &FileWatcher::_on_filesystem_changed));

    set_process(false);
}

void FileWatcher::_process(double delta) {
    uint64_t current_modified_time = DirAccess::get_modified_time(file_path);
    if (current_modified_time != last_modified_time) {
        last_modified_time = current_modified_time;
        _on_filesystem_changed();
    }
}

void FileWatcher::_ensure_file_exists() {
    if (FileAccess::file_exists(file_path)) {
        return;
    }

    Ref<DirAccess> dir = DirAccess::create_for_path("res://react_native");
    if (dir.is_null()) {
        dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    }
    if (!dir->dir_exists("res://react_native")) {
        dir->make_dir_recursive("res://react_native");
    }

    Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::WRITE);
    ERR_FAIL_COND_MSG(file.is_null(), "Failed to create watch file at " + file_path);

    file->store_string("{\"created_by\":\"react_native\",\"timestamp\":" + String::num_int64(OS::get_singleton()->get_unix_time_from_system()) + "}");
    file->flush();
}

void FileWatcher::_on_filesystem_changed() {
    print_line("FileWatcher: Detected change in " + file_path);
}
