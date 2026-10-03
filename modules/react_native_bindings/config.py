def can_build(env, platform):
    env.module_add_dependencies("react_native_bindings", ["text_server_adv", "jpg", "webp"])
    return True


def configure(env):
    pass
