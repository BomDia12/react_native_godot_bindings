#pragma once

#include "scene/resources/material.h"

#include <react/renderer/graphics/Filter.h>

#include <vector>

// A CSS filter chain as shader stages. Consecutive color functions fold into
// one 4x5 color matrix; blur and drop-shadow stay stages of their own.
struct RNFilterStage {
	enum Kind {
		COLOR_MATRIX,
		BLUR,
		DROP_SHADOW,
	};

	Kind kind = COLOR_MATRIX;
	// Rows r, g, b, a; columns r, g, b, a and offset.
	float matrix[4][5] = {
		{ 1, 0, 0, 0, 0 },
		{ 0, 1, 0, 0, 0 },
		{ 0, 0, 1, 0, 0 },
		{ 0, 0, 0, 1, 0 },
	};
	float radius = 0;
	Vector2 offset;
	Color color;

	// Applies p_next after this matrix.
	void then(const float p_next[4][5]);
};

// Each drop shadow doubles the layers the filter shader samples.
constexpr int RN_MAX_DROP_SHADOWS = 3;

// Drop shadows past RN_MAX_DROP_SHADOWS are left out.
Vector<RNFilterStage> rn_filter_stages(const std::vector<facebook::react::FilterFunction> &p_filter);
// How far filtered pixels reach outside the unfiltered ones.
real_t rn_filter_extent(const Vector<RNFilterStage> &p_stages);
bool rn_filter_blurs(const Vector<RNFilterStage> &p_stages);
// The canvas group shader for this sequence of stage kinds.
String rn_filter_shader(const Vector<RNFilterStage> &p_stages);
void rn_set_filter_uniforms(ShaderMaterial *p_material, const Vector<RNFilterStage> &p_stages);
