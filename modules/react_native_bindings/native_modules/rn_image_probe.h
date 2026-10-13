#pragma once

#include "core/math/vector2i.h"

#include <cstddef>
#include <cstdint>

enum class RNImageFormat {
	UNKNOWN,
	PNG,
	JPEG,
	WEBP,
};

struct RNImageHeader {
	RNImageFormat format = RNImageFormat::UNKNOWN;
	Size2i size;
};

// Reads the encoded dimensions without decoding, so callers can reject images before the codec allocates pixels.
bool rn_probe_image(const uint8_t *p_data, size_t p_size, RNImageHeader &r_header);
