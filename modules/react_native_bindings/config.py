def can_build(env, platform):
    env.module_add_dependencies("react_native_bindings", ["text_server_adv", "jpg", "webp", "websocket", "mbedtls"])
    for option in ("disable_http", "disable_tls"):
        if env.get(option, False):
            raise RuntimeError("React Native services require usable HTTP and TLS facilities: " + option)
    return True


def configure(env):
    pass
