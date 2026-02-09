// src/nfc/nfc_allowlist.h
// Role: NFC allowlist storage interface (M6 slice 1).
#pragma once

#include <Arduino.h>

class WssEventLogger;

enum WssNfcRole {
  WSS_NFC_ROLE_UNKNOWN = 0,
  WSS_NFC_ROLE_ADMIN = 1,
  WSS_NFC_ROLE_USER = 2,
};

// Loads allowlist from persistent storage (SD preferred, NVS fallback).
bool wss_nfc_allowlist_begin(WssEventLogger* log);

// M6: per-device-salted, non-reversible tag identifier.
String wss_nfc_taghash(const uint8_t* uid, size_t uid_len);

// Allowlist queries (provisioning arrives later).
bool wss_nfc_allowlist_is_allowed(const String& taghash);
WssNfcRole wss_nfc_allowlist_get_role(const String& taghash);
bool wss_nfc_allowlist_has_admin();
const char* wss_nfc_role_to_string(WssNfcRole role);

// Provisioning operations.
bool wss_nfc_allowlist_add(const String& taghash, WssNfcRole role, WssEventLogger* log);
bool wss_nfc_allowlist_remove(const String& taghash, WssEventLogger* log);

// Allowlist persistence and provisioning arrive in later milestones.
// This hook exists so Factory Restore can explicitly clear allowlist state.
void wss_nfc_allowlist_factory_reset(WssEventLogger& log);
