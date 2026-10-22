#include "rn_filter.h"

#include "../mounting/rn_component_view.h"

using namespace facebook::react;

namespace {

using Matrix = float[4][5];

void identity(Matrix &r_matrix) {
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 5; ++column) {
			r_matrix[row][column] = row == column ? 1 : 0;
		}
	}
}

void rgb(Matrix &r_matrix, const float p_rgb[3][3]) {
	identity(r_matrix);
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 3; ++column) {
			r_matrix[row][column] = p_rgb[row][column];
		}
	}
}

// Filter Effects 1, section 13: the shorthand filter functions as matrices.
void color_matrix(FilterType p_type, float p_amount, Matrix &r_matrix) {
	identity(r_matrix);
	switch (p_type) {
		case FilterType::Brightness: {
			for (int i = 0; i < 3; ++i) {
				r_matrix[i][i] = p_amount;
			}
		} break;
		case FilterType::Contrast: {
			for (int i = 0; i < 3; ++i) {
				r_matrix[i][i] = p_amount;
				r_matrix[i][4] = 0.5f - 0.5f * p_amount;
			}
		} break;
		case FilterType::Invert: {
			const float amount = CLAMP(p_amount, 0.0f, 1.0f);
			for (int i = 0; i < 3; ++i) {
				r_matrix[i][i] = 1 - 2 * amount;
				r_matrix[i][4] = amount;
			}
		} break;
		case FilterType::Opacity: {
			r_matrix[3][3] = CLAMP(p_amount, 0.0f, 1.0f);
		} break;
		case FilterType::Grayscale: {
			const float s = 1 - CLAMP(p_amount, 0.0f, 1.0f);
			const float m[3][3] = {
				{ 0.2126f + 0.7874f * s, 0.7152f - 0.7152f * s, 0.0722f - 0.0722f * s },
				{ 0.2126f - 0.2126f * s, 0.7152f + 0.2848f * s, 0.0722f - 0.0722f * s },
				{ 0.2126f - 0.2126f * s, 0.7152f - 0.7152f * s, 0.0722f + 0.9278f * s },
			};
			rgb(r_matrix, m);
		} break;
		case FilterType::Sepia: {
			const float s = 1 - CLAMP(p_amount, 0.0f, 1.0f);
			const float m[3][3] = {
				{ 0.393f + 0.607f * s, 0.769f - 0.769f * s, 0.189f - 0.189f * s },
				{ 0.349f - 0.349f * s, 0.686f + 0.314f * s, 0.168f - 0.168f * s },
				{ 0.272f - 0.272f * s, 0.534f - 0.534f * s, 0.131f + 0.869f * s },
			};
			rgb(r_matrix, m);
		} break;
		case FilterType::Saturate: {
			const float s = MAX(p_amount, 0.0f);
			const float m[3][3] = {
				{ 0.213f + 0.787f * s, 0.715f - 0.715f * s, 0.072f - 0.072f * s },
				{ 0.213f - 0.213f * s, 0.715f + 0.285f * s, 0.072f - 0.072f * s },
				{ 0.213f - 0.213f * s, 0.715f - 0.715f * s, 0.072f + 0.928f * s },
			};
			rgb(r_matrix, m);
		} break;
		case FilterType::HueRotate: {
			const float c = Math::cos(Math::deg_to_rad(p_amount));
			const float s = Math::sin(Math::deg_to_rad(p_amount));
			const float m[3][3] = {
				{ 0.213f + c * 0.787f - s * 0.213f, 0.715f - c * 0.715f - s * 0.715f, 0.072f - c * 0.072f + s * 0.928f },
				{ 0.213f - c * 0.213f + s * 0.143f, 0.715f + c * 0.285f + s * 0.140f, 0.072f - c * 0.072f - s * 0.283f },
				{ 0.213f - c * 0.213f - s * 0.787f, 0.715f - c * 0.715f + s * 0.715f, 0.072f + c * 0.928f + s * 0.072f },
			};
			rgb(r_matrix, m);
		} break;
		default:
			break;
	}
}

} // namespace

void RNFilterStage::then(const float p_next[4][5]) {
	float result[4][5];
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 5; ++column) {
			float value = column == 4 ? p_next[row][4] : 0;
			for (int k = 0; k < 4; ++k) {
				value += p_next[row][k] * matrix[k][column];
			}
			result[row][column] = value;
		}
	}
	memcpy(matrix, result, sizeof(matrix));
}

::Vector<RNFilterStage> rn_filter_stages(const std::vector<FilterFunction> &p_filter) {
	::Vector<RNFilterStage> stages;
	int drop_shadows = 0;
	for (const FilterFunction &function : p_filter) {
		if (function.type == FilterType::Blur) {
			RNFilterStage stage;
			stage.kind = RNFilterStage::BLUR;
			stage.radius = MAX(0.0f, std::get<Float>(function.parameters));
			stages.push_back(stage);
		} else if (function.type == FilterType::DropShadow) {
			if (++drop_shadows > RN_MAX_DROP_SHADOWS) {
				continue;
			}
			const DropShadowParams &shadow = std::get<DropShadowParams>(function.parameters);
			RNFilterStage stage;
			stage.kind = RNFilterStage::DROP_SHADOW;
			stage.radius = MAX(0.0f, shadow.standardDeviation);
			stage.offset = Vector2(shadow.offsetX, shadow.offsetY);
			stage.color = rn_godot_color(shadow.color, ::Color(0, 0, 0));
			stages.push_back(stage);
		} else {
			float matrix[4][5];
			color_matrix(function.type, std::get<Float>(function.parameters), matrix);
			if (stages.is_empty() || stages[stages.size() - 1].kind != RNFilterStage::COLOR_MATRIX) {
				stages.push_back(RNFilterStage());
			}
			stages.write[stages.size() - 1].then(matrix);
		}
	}
	return stages;
}

real_t rn_filter_extent(const ::Vector<RNFilterStage> &p_stages) {
	real_t extent = 0;
	for (const RNFilterStage &stage : p_stages) {
		if (stage.kind != RNFilterStage::COLOR_MATRIX) {
			extent += stage.radius * 3 + MAX(Math::abs(stage.offset.x), Math::abs(stage.offset.y));
		}
	}
	return Math::ceil(extent);
}

bool rn_filter_blurs(const ::Vector<RNFilterStage> &p_stages) {
	for (const RNFilterStage &stage : p_stages) {
		if (stage.kind != RNFilterStage::COLOR_MATRIX && stage.radius > 0) {
			return true;
		}
	}
	return false;
}

namespace {

// One sample of the canvas group in the composited result: the content
// itself, or a shadow of an earlier layer. Later stages apply to every layer.
struct FilterLayer {
	::Vector<int> shifts;
	::Vector<int> radii;
	// Color matrix stages, or drop-shadow stages that tint the layer, in order.
	::Vector<int> operations;
};

} // namespace

// Stages run in a single pass over the canvas group, so each layer is a
// separate sample: a blur widens every layer's sample, a color stage applies to
// every layer, and a drop shadow adds a tinted, shifted copy of every layer
// behind them, as filtering the previous stage's whole output does.
String rn_filter_shader(const ::Vector<RNFilterStage> &p_stages) {
	String uniforms;
	::Vector<FilterLayer> layers = { FilterLayer() };
	for (int i = 0; i < p_stages.size(); ++i) {
		switch (p_stages[i].kind) {
			case RNFilterStage::COLOR_MATRIX: {
				uniforms += vformat("uniform mat4 stage_matrix_%d;\nuniform vec4 stage_offset_%d;\n", i, i);
				for (FilterLayer &layer : layers) {
					layer.operations.push_back(i);
				}
			} break;
			case RNFilterStage::BLUR: {
				uniforms += vformat("uniform float stage_radius_%d;\n", i);
				for (FilterLayer &layer : layers) {
					layer.radii.push_back(i);
				}
			} break;
			case RNFilterStage::DROP_SHADOW: {
				uniforms += vformat("uniform float stage_radius_%d;\nuniform vec2 stage_shift_%d;\nuniform vec4 stage_color_%d;\n", i, i, i);
				const int count = layers.size();
				for (int j = 0; j < count; ++j) {
					FilterLayer shadow = layers[j];
					shadow.shifts.push_back(i);
					shadow.radii.push_back(i);
					shadow.operations.push_back(i);
					layers.push_back(shadow);
				}
			} break;
		}
	}
	String body;
	for (int j = 0; j < layers.size(); ++j) {
		const FilterLayer &layer = layers[j];
		String shift = "vec2(0.0)";
		for (int stage : layer.shifts) {
			shift += vformat(" + stage_shift_%d", stage);
		}
		String radius = "0.0";
		for (int stage : layer.radii) {
			radius += vformat(" + stage_radius_%d * stage_radius_%d", stage, stage);
		}
		body += vformat("\tvec4 layer_%d = blurred(SCREEN_UV - (%s) * SCREEN_PIXEL_SIZE, sqrt(%s), SCREEN_PIXEL_SIZE);\n", j, shift, radius);
		for (int stage : layer.operations) {
			if (p_stages[stage].kind == RNFilterStage::COLOR_MATRIX) {
				body += vformat("\tlayer_%d = color_matrix(layer_%d, stage_matrix_%d, stage_offset_%d);\n", j, j, stage, stage);
			} else {
				body += vformat("\tlayer_%d = vec4(stage_color_%d.rgb, stage_color_%d.a * layer_%d.a);\n", j, stage, stage, j);
			}
		}
		body += j == 0 ? "\tvec4 c = layer_0;\n" : vformat("\tc = over(c, layer_%d);\n", j);
	}
	return vformat(R"(
shader_type canvas_item;
uniform sampler2D group : hint_screen_texture, repeat_disable, filter_linear_mipmap;
%s
vec4 straight(vec4 c) {
	return c.a > 0.0001 ? vec4(c.rgb / c.a, c.a) : vec4(0.0);
}

vec4 blurred(vec2 uv, float radius, vec2 pixel) {
	if (radius <= 0.0) {
		return straight(texture(group, uv));
	}
	float lod = log2(max(radius, 1.0));
	vec2 spacing = pixel * radius * 0.5;
	vec4 sum = vec4(0.0);
	float weights = 0.0;
	for (int x = -2; x <= 2; x++) {
		for (int y = -2; y <= 2; y++) {
			float w = exp(-float(x * x + y * y) / 4.0);
			sum += textureLod(group, uv + vec2(float(x), float(y)) * spacing, lod) * w;
			weights += w;
		}
	}
	return straight(sum / weights);
}

vec4 color_matrix(vec4 c, mat4 m, vec4 offset) {
	return clamp(m * c + offset, 0.0, 1.0);
}

vec4 over(vec4 top, vec4 bottom) {
	float a = top.a + bottom.a * (1.0 - top.a);
	return a > 0.0 ? vec4((top.rgb * top.a + bottom.rgb * bottom.a * (1.0 - top.a)) / a, a) : vec4(0.0);
}

void fragment() {
%s	COLOR = c;
}
)",
			uniforms, body);
}

void rn_set_filter_uniforms(ShaderMaterial *p_material, const ::Vector<RNFilterStage> &p_stages) {
	for (int i = 0; i < p_stages.size(); ++i) {
		const RNFilterStage &stage = p_stages[i];
		if (stage.kind == RNFilterStage::COLOR_MATRIX) {
			// GLSL matrices are column-major: column j holds input channel j.
			Projection matrix;
			for (int row = 0; row < 4; ++row) {
				for (int column = 0; column < 4; ++column) {
					matrix.columns[column][row] = stage.matrix[row][column];
				}
			}
			p_material->set_shader_parameter(vformat("stage_matrix_%d", i), matrix);
			p_material->set_shader_parameter(vformat("stage_offset_%d", i), Vector4(stage.matrix[0][4], stage.matrix[1][4], stage.matrix[2][4], stage.matrix[3][4]));
		} else {
			p_material->set_shader_parameter(vformat("stage_radius_%d", i), stage.radius);
			if (stage.kind == RNFilterStage::DROP_SHADOW) {
				p_material->set_shader_parameter(vformat("stage_shift_%d", i), stage.offset);
				p_material->set_shader_parameter(vformat("stage_color_%d", i), stage.color);
			}
		}
	}
}
