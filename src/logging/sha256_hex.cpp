// src/logging/sha256_hex.cpp
// Role: Compute SHA-256 hex digests for log hash chaining (M3).

#include "sha256_hex.h"

#include "mbedtls/sha256.h"

static String hex_lower(const uint8_t* bytes, size_t len) {
  static const char* kHex = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out += kHex[(bytes[i] >> 4) & 0x0F];
    out += kHex[bytes[i] & 0x0F];
  }
  return out;
}

String wss_sha256_hex(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    uint8_t empty[32];
    memset(empty, 0, sizeof(empty));
    // SHA-256 of empty is well-defined, but callers here always provide bytes.
  }

  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, data, len);
  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  return hex_lower(digest, sizeof(digest));
}

String wss_sha256_hex_str(const String& s) {
  return wss_sha256_hex(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
}

// src/logging/sha256_hex.cpp EOF
