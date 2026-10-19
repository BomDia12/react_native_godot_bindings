#pragma once

#include "core/os/os.h"
#include "scene/resources/font.h"

// The sample's fixture fonts, found from the editor binary in godot/bin, since
// CI has no system fonts for Arabic or Hebrew.
inline Ref<FontFile> rn_test_font(const String &p_file) {
	const String path = OS::get_singleton()->get_executable_path().get_base_dir().path_join("../../samples/game-ui/assets").path_join(p_file).simplify_path();
	Ref<FontFile> font;
	font.instantiate();
	return font->load_dynamic_font(path) == OK ? font : Ref<FontFile>();
}
