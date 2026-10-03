#include "rn_visual_style.h"

#include "../fabric/rn_view_style.h"

#include <cmath>

namespace {

bool numeric(const Variant &p_value) {
	return (p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT) && std::isfinite(double(p_value));
}

bool length(const Variant &p_value) {
	if (numeric(p_value)) {
		return true;
	}
	if (p_value.get_type() != Variant::STRING) {
		return false;
	}
	const String text = p_value;
	return text.ends_with("%") && text.trim_suffix("%").is_valid_float() && std::isfinite(text.trim_suffix("%").to_float());
}

real_t resolve_length(const Variant &p_value, real_t p_extent) {
	if (p_value.get_type() == Variant::STRING) {
		return String(p_value).trim_suffix("%").to_float() * p_extent / 100;
	}
	return real_t(p_value);
}

bool angle(const Variant &p_value) {
	if (p_value.get_type() != Variant::STRING) {
		return false;
	}
	const String text = p_value;
	return (text.ends_with("deg") || text.ends_with("rad")) && text.left(-3).is_valid_float() && std::isfinite(text.left(-3).to_float());
}

real_t resolve_angle(const Variant &p_value) {
	const String text = p_value;
	const real_t value = text.left(-3).to_float();
	return text.ends_with("deg") ? Math::deg_to_rad(value) : value;
}

bool valid_matrix(const Variant &p_value) {
	if (p_value.get_type() != Variant::ARRAY) {
		return false;
	}
	const Array matrix = p_value;
	if (matrix.size() != 6 && matrix.size() != 16) {
		return false;
	}
	for (int i = 0; i < matrix.size(); ++i) {
		if (!numeric(matrix[i])) {
			return false;
		}
	}
	if (matrix.size() == 16) {
		for (int i : { 2, 3, 6, 7, 8, 9, 11, 14 }) {
			if (double(matrix[i]) != 0) {
				return false;
			}
		}
		return double(matrix[10]) == 1 && double(matrix[15]) == 1;
	}
	return true;
}

Transform2D operation(const String &p_key, const Variant &p_value, const Size2 &p_size) {
	Transform2D result;
	if (p_key == "matrix") {
		const Array m = p_value;
		const int y = m.size() == 6 ? 2 : 4;
		const int origin = m.size() == 6 ? 4 : 12;
		return Transform2D(real_t(m[0]), real_t(m[1]), real_t(m[y]), real_t(m[y + 1]), real_t(m[origin]), real_t(m[origin + 1]));
	}
	if (p_key == "rotate" || p_key == "rotateZ") {
		return Transform2D(resolve_angle(p_value), Point2());
	}
	if (p_key == "skewX") {
		result[1].x = std::tan(resolve_angle(p_value));
	} else if (p_key == "skewY") {
		result[0].y = std::tan(resolve_angle(p_value));
	} else if (p_key == "scale") {
		result.scale_basis(Vector2(real_t(p_value), real_t(p_value)));
	} else if (p_key == "scaleX") {
		result[0].x = real_t(p_value);
	} else if (p_key == "scaleY") {
		result[1].y = real_t(p_value);
	} else if (p_key == "translateX") {
		result[2].x = resolve_length(p_value, p_size.x);
	} else if (p_key == "translateY") {
		result[2].y = resolve_length(p_value, p_size.y);
	} else if (p_key == "translate") {
		const Array translation = p_value;
		result[2] = Point2(resolve_length(translation[0], p_size.x), resolve_length(translation[1], p_size.y));
	}
	return result;
}

} // namespace

bool RNVisualStyle::validate(const Dictionary &p_props, RNError &r_error) {
	for (const Variant &key_value : p_props.keys()) {
		const String key = key_value;
		if (key == "zIndex" || key == "opacity" || (key.begins_with("border") && (key.ends_with("Width") || key.ends_with("Radius")))) {
			const Variant value = p_props[key];
			if (!numeric(value) || Math::abs(double(value)) > 1048576 || (key != "zIndex" && double(value) < 0) || (key == "opacity" && double(value) > 1)) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "Style value must be finite and within its range", "host.prepare", "props." + key);
				return false;
			}
		}
	}
	if (String(p_props.get("borderStyle", "solid")) != "solid") {
		r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Only solid borders are supported", "host.prepare", "props.borderStyle");
		return false;
	}
	if (p_props.has("boxShadow")) {
		const Variant shadow_value = p_props["boxShadow"];
		if (shadow_value.get_type() != Variant::ARRAY || Array(shadow_value).size() > 1) {
			r_error = RNError::make(RNErrorCode::UNSUPPORTED, "Only one outset boxShadow is supported", "host.prepare", "props.boxShadow");
			return false;
		}
		const Array shadows = shadow_value;
		if (!shadows.is_empty()) {
			if (shadows[0].get_type() != Variant::DICTIONARY) {
				r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid boxShadow", "host.prepare");
				return false;
			}
			const Dictionary shadow = shadows[0];
			Color ignored;
			if (bool(shadow.get("inset", false)) || (shadow.has("color") && !RNViewStyle::color_of(shadow, "color", ignored))) {
				r_error = RNError::make(RNErrorCode::UNSUPPORTED, "boxShadow requires an outset shadow and a valid color", "host.prepare");
				return false;
			}
			for (const char *key : { "offsetX", "offsetY", "blurRadius", "spreadDistance" }) {
				const Variant value = shadow.get(key, 0.0);
				if (!numeric(value) || Math::abs(double(value)) > 4096 || (String(key) == "blurRadius" && double(value) < 0)) {
					r_error = RNError::make(RNErrorCode::VALIDATION, "Invalid bounded shadow dimensions", "host.prepare");
					return false;
				}
			}
		}
	}
	auto reject = [&](const String &p_path) {
		r_error = RNError::make(RNErrorCode::VALIDATION, "Expected a finite 2D affine transform", "host.prepare", p_path);
		return false;
	};
	const Variant value = p_props.get("transform", Array());
	if (value.get_type() != Variant::ARRAY) {
		return reject("props.transform");
	}
	const Array transforms = value;
	if (transforms.size() > 256) {
		return reject("props.transform");
	}
	for (int i = 0; i < transforms.size(); ++i) {
		if (transforms[i].get_type() != Variant::DICTIONARY) {
			return reject("props.transform");
		}
		const Dictionary entry = transforms[i];
		if (entry.size() != 1) {
			return reject("props.transform");
		}
		const String key = entry.keys()[0];
		const Variant v = entry[key];
		bool valid = false;
		if (key == "matrix") {
			valid = valid_matrix(v);
		} else if (key == "rotate" || key == "rotateZ" || key == "skewX" || key == "skewY") {
			valid = angle(v);
		} else if (key == "scale" || key == "scaleX" || key == "scaleY") {
			valid = numeric(v);
		} else if (key == "translateX" || key == "translateY") {
			valid = length(v);
		} else if (key == "translate" && v.get_type() == Variant::ARRAY) {
			const Array translation = v;
			valid = (translation.size() == 2 || translation.size() == 3) && length(translation[0]) && length(translation[1]) && (translation.size() == 2 || (numeric(translation[2]) && double(translation[2]) == 0));
		}
		if (!valid) {
			return reject("props.transform." + key);
		}
	}
	if (p_props.has("transformOrigin")) {
		const Variant origin_value = p_props["transformOrigin"];
		if (origin_value.get_type() != Variant::ARRAY) {
			return reject("props.transformOrigin");
		}
		const Array origin = origin_value;
		if ((origin.size() != 2 && origin.size() != 3) || !length(origin[0]) || !length(origin[1]) || (origin.size() == 3 && (!numeric(origin[2]) || double(origin[2]) != 0))) {
			return reject("props.transformOrigin");
		}
	}
	if (!transform(p_props, Size2(1, 1)).is_finite()) {
		return reject("props.transform");
	}
	return true;
}

Transform2D RNVisualStyle::transform(const Dictionary &p_props, const Size2 &p_size) {
	Transform2D result;
	const Array transforms = p_props.get("transform", Array());
	for (int i = 0; i < transforms.size(); ++i) {
		const Dictionary entry = transforms[i];
		const String key = entry.keys()[0];
		result *= operation(key, entry[key], p_size);
	}
	Point2 origin = p_size / 2;
	if (p_props.has("transformOrigin")) {
		const Array value = p_props["transformOrigin"];
		origin = Point2(resolve_length(value[0], p_size.x), resolve_length(value[1], p_size.y));
	}
	return Transform2D(0, origin) * result * Transform2D(0, -origin);
}

void RNVisualStyle::apply(Control *p_host, const Dictionary &p_props) {
	p_host->set_affine_transform(transform(p_props, p_host->get_size()));
	const String direction = p_props.get("direction", "inherit");
	p_host->set_layout_direction(direction == "rtl" ? Control::LAYOUT_DIRECTION_RTL : direction == "ltr" ? Control::LAYOUT_DIRECTION_LTR
																										 : Control::LAYOUT_DIRECTION_INHERITED);
}
