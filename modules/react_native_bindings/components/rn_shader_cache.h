#pragma once

#include "scene/resources/shader.h"

// Shaders are generated from source and shared by every material with the
// same source. The cache is cleared before the rendering server goes away.
Ref<Shader> rn_cached_shader(const String &p_code);
void rn_clear_shader_cache();
