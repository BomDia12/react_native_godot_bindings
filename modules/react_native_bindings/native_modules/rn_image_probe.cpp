#include "rn_image_probe.h"

#include "core/io/image.h"

#include <cstring>

namespace {

uint32_t read_be16(const uint8_t *p_data) {
	return (uint32_t(p_data[0]) << 8) | p_data[1];
}

uint32_t read_be32(const uint8_t *p_data) {
	return (read_be16(p_data) << 16) | read_be16(p_data + 2);
}

uint32_t read_le16(const uint8_t *p_data) {
	return uint32_t(p_data[0]) | (uint32_t(p_data[1]) << 8);
}

uint32_t read_le24(const uint8_t *p_data) {
	return read_le16(p_data) | (uint32_t(p_data[2]) << 16);
}

uint32_t read_le32(const uint8_t *p_data) {
	return read_le24(p_data) | (uint32_t(p_data[3]) << 24);
}

bool probe_png(const uint8_t *p_data, size_t p_size, Size2i &r_size) {
	if (p_size < 24 || memcmp(p_data + 12, "IHDR", 4) != 0) {
		return false;
	}
	r_size = Size2i(read_be32(p_data + 16), read_be32(p_data + 20));
	return true;
}

bool is_start_of_frame(uint8_t p_marker) {
	return p_marker >= 0xC0 && p_marker <= 0xCF && p_marker != 0xC4 && p_marker != 0xC8 && p_marker != 0xCC;
}

bool probe_jpeg(const uint8_t *p_data, size_t p_size, Size2i &r_size) {
	size_t position = 2;
	while (position < p_size) {
		if (p_data[position] != 0xFF) {
			return false;
		}
		while (position < p_size && p_data[position] == 0xFF) {
			++position;
		}
		if (position >= p_size) {
			return false;
		}
		const uint8_t marker = p_data[position++];
		const bool standalone = marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8);
		if (standalone) {
			continue;
		}
		if (marker == 0xD9 || marker == 0xDA || position + 2 > p_size) {
			return false;
		}
		const size_t length = read_be16(p_data + position);
		if (length < 2 || position + length > p_size) {
			return false;
		}
		if (is_start_of_frame(marker)) {
			if (length < 7) {
				return false;
			}
			r_size = Size2i(read_be16(p_data + position + 5), read_be16(p_data + position + 3));
			return true;
		}
		position += length;
	}
	return false;
}

bool probe_webp(const uint8_t *p_data, size_t p_size, Size2i &r_size) {
	if (p_size < 30) {
		return false;
	}
	const uint8_t *chunk = p_data + 12;
	const uint8_t *payload = p_data + 20;
	if (memcmp(chunk, "VP8 ", 4) == 0) {
		if (payload[3] != 0x9D || payload[4] != 0x01 || payload[5] != 0x2A) {
			return false;
		}
		r_size = Size2i(read_le16(payload + 6) & 0x3FFF, read_le16(payload + 8) & 0x3FFF);
		return true;
	}
	if (memcmp(chunk, "VP8L", 4) == 0) {
		if (payload[0] != 0x2F) {
			return false;
		}
		const uint32_t bits = read_le32(payload + 1);
		r_size = Size2i((bits & 0x3FFF) + 1, ((bits >> 14) & 0x3FFF) + 1);
		return true;
	}
	if (memcmp(chunk, "VP8X", 4) == 0) {
		r_size = Size2i(read_le24(payload + 4) + 1, read_le24(payload + 7) + 1);
		return true;
	}
	return false;
}

} // namespace

bool rn_probe_image(const uint8_t *p_data, size_t p_size, RNImageHeader &r_header) {
	r_header = RNImageHeader();
	if (!p_data || p_size < 12) {
		return false;
	}
	Size2i size;
	bool probed = false;
	if (memcmp(p_data, "\x89PNG\r\n\x1a\n", 8) == 0) {
		r_header.format = RNImageFormat::PNG;
		probed = probe_png(p_data, p_size, size);
	} else if (p_data[0] == 0xFF && p_data[1] == 0xD8) {
		r_header.format = RNImageFormat::JPEG;
		probed = probe_jpeg(p_data, p_size, size);
	} else if (memcmp(p_data, "RIFF", 4) == 0 && memcmp(p_data + 8, "WEBP", 4) == 0) {
		r_header.format = RNImageFormat::WEBP;
		probed = probe_webp(p_data, p_size, size);
	}
	if (!probed || size.x <= 0 || size.y <= 0 || size.x > Image::MAX_WIDTH || size.y > Image::MAX_HEIGHT) {
		r_header = RNImageHeader();
		return false;
	}
	r_header.size = size;
	return true;
}
