// src/nfc/nfc_allowlist.cpp
// Role: NFC allowlist storage interface (M6 slice 1).

#include "nfc_allowlist.h"

#include <Arduino.h>

#include "../logging/event_logger.h"
#include "../logging/sha256_hex.h"

namespace {

struct AllowEntry {
  const char* taghash;
  WssNfcRole role;
};

// Slice 1: no provisioning yet, allowlist is empty by default.
static const AllowEntry kAllowlist[] = {};
static const size_t kAllowlistCount = sizeof(kAllowlist) / sizeof(kAllowlist[0]);

static uint64_t device_salt() {
  return ESP.getEfuseMac();
}

} // namespace

String wss_nfc_taghash(const uint8_t* uid, size_t uid_len) {
  if (!uid || uid_len == 0) return String();
  uint8_t* data = nullptr;
  size_t len = 0;
  uint8_t stack_buf[8 + 16];
  if (uid_len <= 16) {
    data = stack_buf;
    len = 8 + uid_len;
  } else {
    data = new uint8_t[8 + uid_len];
    len = 8 + uid_len;
  }

  uint64_t salt = device_salt();
  for (int i = 0; i < 8; i++) {
    data[i] = (uint8_t)((salt >> (56 - i * 8)) & 0xFF);
  }
  memcpy(data + 8, uid, uid_len);
  String out = wss_sha256_hex(data, len);
  if (data != stack_buf) {
    delete[] data;
  }
  return out;
}

bool wss_nfc_allowlist_is_allowed(const String& taghash) {
  return wss_nfc_allowlist_get_role(taghash) != WSS_NFC_ROLE_UNKNOWN;
}

WssNfcRole wss_nfc_allowlist_get_role(const String& taghash) {
  if (!taghash.length()) return WSS_NFC_ROLE_UNKNOWN;
  for (size_t i = 0; i < kAllowlistCount; i++) {
    if (taghash == kAllowlist[i].taghash) {
      return kAllowlist[i].role;
    }
  }
  return WSS_NFC_ROLE_UNKNOWN;
}

const char* wss_nfc_role_to_string(WssNfcRole role) {
  switch (role) {
    case WSS_NFC_ROLE_ADMIN:
      return "admin";
    case WSS_NFC_ROLE_USER:
      return "user";
    case WSS_NFC_ROLE_UNKNOWN:
    default:
      return "unknown";
  }
}

void wss_nfc_allowlist_factory_reset(WssEventLogger& log) {
  // M1 stub: nothing to clear yet, but make behavior observable.
  log.log_info("nfc", "allowlist_factory_reset_stub", "allowlist reset (stub in M1)");
}
