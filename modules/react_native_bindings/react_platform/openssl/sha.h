#pragma once

// Upstream DevServerHelper hashes the device identity with OpenSSL; Godot ships
// mbedTLS, so this header provides the same calls over Godot's CryptoCore.

#include "core/crypto/crypto_core.h"

#include <cstddef>

#define SHA256_DIGEST_LENGTH 32

using SHA256_CTX = CryptoCore::SHA256Context;

inline int SHA256_Init(SHA256_CTX *p_context) {
	return p_context->start() == OK;
}

inline int SHA256_Update(SHA256_CTX *p_context, const void *p_data, size_t p_length) {
	return p_context->update(static_cast<const uint8_t *>(p_data), p_length) == OK;
}

inline int SHA256_Final(unsigned char *p_digest, SHA256_CTX *p_context) {
	return p_context->finish(p_digest) == OK;
}
