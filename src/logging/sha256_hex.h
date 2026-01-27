// src/logging/sha256_hex.h
// Role: Compute SHA-256 hex digests for log hash chaining (M3).
#pragma once

#include <Arduino.h>

// Returns lowercase hex SHA-256 of the provided bytes.
String wss_sha256_hex(const uint8_t* data, size_t len);

// Convenience wrapper for Arduino String input.
String wss_sha256_hex_str(const String& s);

// src/logging/sha256_hex.h EOF
