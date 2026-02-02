// src/nfc/nfc_manager.h
// Role: NFC health + scan logging (M6 slice 0). No auth/state logic.
//
// Contract anchors:
// - docs/State_Machine_v1_0.md (NFC optional, degrade gracefully)
// - docs/Event_Log_Schema_v1_0.md (nfc_scan events)
// - docs/Assumptions_Registry.md (NFC absence/degraded explicit)

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class WssConfigStore;
class WssEventLogger;

struct WssNfcStatus {
  bool feature_enabled = false;   // build-time NFC feature flag
  bool enabled_cfg = false;       // control_nfc_enabled
  bool reader_present = false;    // best-effort (true only if reader init succeeds)
  String health;                  // ok|disabled_cfg|disabled_build|unavailable
  String driver;                  // stub|none (driver name later)
  String last_role;               // admin|user|unknown
  String last_scan_result;        // ok|fail
  String last_scan_reason;        // allowlist_match|allowlist_unknown|reader_unavailable|uid_invalid
  uint32_t last_scan_ms = 0;
  uint32_t last_scan_ok_ms = 0;
  uint32_t last_scan_fail_ms = 0;
  uint32_t scan_ok_count = 0;
  uint32_t scan_fail_count = 0;
};

void wss_nfc_begin(WssConfigStore* cfg, WssEventLogger* log);
void wss_nfc_loop();
void wss_nfc_on_uid(const uint8_t* uid, size_t uid_len);
WssNfcStatus wss_nfc_status();
void wss_nfc_write_status_json(JsonObject out);
