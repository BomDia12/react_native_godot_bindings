def can_build(env, platform):
    env.module_add_dependencies("rn_godot_example", ["react_native_bindings"])
    return True


def configure(env):
    pass
