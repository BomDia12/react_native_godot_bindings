#include "rn_shader_cache.h"

#include "core/os/thread.h"
#include "core/templates/hash_map.h"

namespace {

HashMap<String, Ref<Shader>> &shaders() {
	static HashMap<String, Ref<Shader>> cache;
	return cache;
}

} // namespace

Ref<Shader> rn_cached_shader(const String &p_code) {
	ERR_FAIL_COND_V(!Thread::is_main_thread(), Ref<Shader>());
	if (const Ref<Shader> *cached = shaders().getptr(p_code)) {
		return *cached;
	}
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	shaders().insert(p_code, shader);
	return shader;
}

void rn_clear_shader_cache() {
	shaders().clear();
}
