#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

#include <cxxreact/JSBigString.h>
#include <react/io/ResourceLoader.h>

namespace facebook::react {

namespace {

bool is_godot_path(const std::string &p_path) {
	return p_path.starts_with("res://") || p_path.starts_with("user://");
}

} // namespace

bool ResourceLoader::isResourceDirectory(const std::string &p_path) {
	return is_godot_path(p_path) && DirAccess::exists(String::utf8(p_path.c_str()));
}

bool ResourceLoader::isResourceFile(const std::string &p_path) {
	return is_godot_path(p_path) && FileAccess::exists(String::utf8(p_path.c_str()));
}

std::unique_ptr<const JSBigString> ResourceLoader::getResourceFileContents(const std::string &p_path) {
	const Vector<uint8_t> bytes = FileAccess::get_file_as_bytes(String::utf8(p_path.c_str()));
	return std::make_unique<JSBigStdString>(std::string(reinterpret_cast<const char *>(bytes.ptr()), bytes.size()));
}

std::filesystem::path ResourceLoader::getCacheRootPath() {
	const String root = ProjectSettings::get_singleton()->globalize_path("user://react_native/cache");
	DirAccess::make_dir_recursive_absolute(root);
	return std::filesystem::path(root.utf8().get_data());
}

} // namespace facebook::react
