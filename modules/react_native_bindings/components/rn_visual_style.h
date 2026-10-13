#pragma once

#include "../interop/rn_error.h"

#include "scene/gui/control.h"

class RNVisualStyle {
public:
	static bool validate(const Dictionary &p_props, RNError &r_error);
	static Transform2D transform_matrix(const Dictionary &p_props, const Size2 &p_size);
	static Point2 transform_origin(const Dictionary &p_props, const Size2 &p_size);
	static void apply(Control *p_host, const Dictionary &p_props);
};
