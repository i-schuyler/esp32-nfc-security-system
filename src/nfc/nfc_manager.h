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
  String health_state;            // ok|unknown|fault
  String interface;               // spi|i2c
  String last_error;              // last reader error (no secrets)
  String driver;                  // stub|none (driver name later)
  bool driver_active = false;
  int spi_cs_gpio = -1;
  int spi_irq_gpio = -1;
  int spi_rst_gpio = -1;
  String last_role;               // admin|user|unknown
  String last_scan_result;        // ok|fail
  String last_scan_reason;        // allowlist_match|allowlist_unknown|reader_unavailable|uid_invalid
  bool lockout_active = false;
  uint32_t lockout_remaining_s = 0;
  String lockout_until_ts;
  bool hold_active = false;
  bool hold_ready = false;
  uint32_t hold_progress_s = 0;
  bool provisioning_active = false;
  String provisioning_mode; // add_user|add_admin|remove|none
  uint32_t provisioning_remaining_s = 0;
  bool admin_eligible_active = false;
  uint32_t admin_eligible_remaining_s = 0;
  String last_writeback_result; // ok|fail|truncated
  String last_writeback_reason;
  String last_writeback_ts;     // ISO-8601 or "u"
  uint32_t last_scan_ms = 0;
  uint32_t last_scan_ok_ms = 0;
  uint32_t last_scan_fail_ms = 0;
  uint32_t scan_ok_count = 0;
  uint32_t scan_fail_count = 0;
};

void wss_nfc_begin(WssConfigStore* cfg, WssEventLogger* log);
void wss_nfc_loop();
void wss_nfc_on_uid(const uint8_t* uid, size_t uid_len);
bool wss_nfc_provision_start(const char* mode);
bool wss_nfc_provision_set_mode(const char* mode);
void wss_nfc_provision_stop(const char* reason);
WssNfcStatus wss_nfc_status();
void wss_nfc_write_status_json(JsonObject out);
bool wss_nfc_admin_gate_required();
bool wss_nfc_admin_eligible_active();
uint32_t wss_nfc_admin_eligible_remaining_s();
void wss_nfc_admin_eligible_clear(const char* reason);
