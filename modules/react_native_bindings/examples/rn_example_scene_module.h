#pragma once

#include "../native_modules/rn_native_module_registry.h"

#include "scene/main/node.h"

class RNExampleCounter : public Node {
	GDCLASS(RNExampleCounter, Node);

	NodePath root_view_path;
	double value = 0;
	Vector2 position;
	Color tint = Color(0.2, 0.6, 1.0, 1.0);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_root_view_path(const NodePath &p_path) { root_view_path = p_path; }
	NodePath get_root_view_path() const { return root_view_path; }
	Dictionary read() const;
	Dictionary increment(double p_amount);
};

bool rn_register_example_scene_module(RNNativeModuleRegistry &p_registry, RNError &r_error);
