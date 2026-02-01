// src/nfc/nfc_manager.cpp
// Role: NFC health + scan logging (M6 slice 0). No auth/state logic.

#include "nfc_manager.h"

#include <Arduino.h>

#include "../config/config_store.h"
#include "../logging/event_logger.h"

namespace {

static WssConfigStore* g_cfg = nullptr;
static WssEventLogger* g_log = nullptr;
static WssNfcStatus g_status;
static uint32_t g_last_poll_ms = 0;
static bool g_logged_unavailable = false;
static bool g_last_enabled_cfg = true;

static bool feature_enabled() {
#if defined(WSS_FEATURE_NFC) && WSS_FEATURE_NFC
  return true;
#else
  return false;
#endif
}

static bool cfg_bool(const char* k, bool def) {
  if (!g_cfg) return def;
  return g_cfg->doc()[k] | def;
}

static void set_health_disabled_build() {
  g_status.health = "disabled_build";
  g_status.reader_present = false;
  g_status.driver = "none";
}

static void set_health_disabled_cfg() {
  g_status.health = "disabled_cfg";
  g_status.reader_present = false;
  if (g_status.feature_enabled && g_status.driver.length() == 0) {
    g_status.driver = "stub";
  }
}

static void set_health_unavailable() {
  g_status.health = "unavailable";
  g_status.reader_present = false;
  if (g_status.driver.length() == 0) {
    g_status.driver = "stub";
  }
}

static void log_scan_event(bool ok, const char* reason) {
  uint32_t now_ms = millis();
  g_status.last_scan_ms = now_ms;
  if (ok) {
    g_status.last_scan_ok_ms = now_ms;
    g_status.scan_ok_count++;
  } else {
    g_status.last_scan_fail_ms = now_ms;
    g_status.scan_fail_count++;
  }
  if (!g_log) return;
  StaticJsonDocument<192> extra;
  extra["result"] = ok ? "ok" : "fail";
  if (reason && reason[0]) extra["reason"] = reason;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  if (ok) {
    g_log->log_info("nfc", "nfc_scan", "nfc scan ok", &o);
  } else {
    g_log->log_warn("nfc", "nfc_scan", "nfc scan failed", &o);
  }
}

} // namespace

void wss_nfc_begin(WssConfigStore* cfg, WssEventLogger* log) {
  g_cfg = cfg;
  g_log = log;
  g_status = WssNfcStatus();
  g_status.feature_enabled = feature_enabled();
  g_status.enabled_cfg = cfg_bool("control_nfc_enabled", true);
  g_last_enabled_cfg = g_status.enabled_cfg;
  g_last_poll_ms = 0;
  g_logged_unavailable = false;

  if (!g_status.feature_enabled) {
    set_health_disabled_build();
    return;
  }

  g_status.driver = "stub";
  if (!g_status.enabled_cfg) {
    set_health_disabled_cfg();
    return;
  }

  // Slice 0 does not bind a reader driver yet. Mark unavailable and log once.
  set_health_unavailable();
  if (g_log && !g_logged_unavailable) {
    g_logged_unavailable = true;
    g_log->log_warn("nfc", "nfc_unavailable", "nfc reader unavailable (driver not initialized)");
  }
}

void wss_nfc_loop() {
  if (!g_cfg) return;

  g_status.feature_enabled = feature_enabled();
  g_status.enabled_cfg = cfg_bool("control_nfc_enabled", true);

  if (!g_status.feature_enabled) {
    set_health_disabled_build();
    return;
  }

  if (!g_status.enabled_cfg) {
    if (g_last_enabled_cfg) {
      g_status.scan_ok_count = 0;
      g_status.scan_fail_count = 0;
    }
    g_last_enabled_cfg = false;
    set_health_disabled_cfg();
    return;
  }

  if (!g_last_enabled_cfg) {
    g_last_enabled_cfg = true;
    g_logged_unavailable = false;
  }

  // Slice 0: no reader driver yet. Keep status non-blocking.
  set_health_unavailable();

  uint32_t now_ms = millis();
  static const uint32_t kPollIntervalMs = 150;
  if ((uint32_t)(now_ms - g_last_poll_ms) < kPollIntervalMs) return;
  g_last_poll_ms = now_ms;

  // Placeholder for future driver poll. Emit a low-rate scan failure when reader is unavailable.
  static const uint32_t kUnavailableLogIntervalMs = 30000;
  if (g_status.last_scan_fail_ms == 0 ||
      (uint32_t)(now_ms - g_status.last_scan_fail_ms) >= kUnavailableLogIntervalMs) {
    log_scan_event(false, "reader_unavailable");
  }
}

WssNfcStatus wss_nfc_status() {
  return g_status;
}

void wss_nfc_write_status_json(JsonObject out) {
  WssNfcStatus st = wss_nfc_status();
  out["feature_enabled"] = st.feature_enabled;
  out["enabled_cfg"] = st.enabled_cfg;
  out["health"] = st.health;
  out["reader_present"] = st.reader_present;
  out["driver"] = st.driver;
  out["last_scan_ms"] = st.last_scan_ms;
  out["last_scan_ok_ms"] = st.last_scan_ok_ms;
  out["last_scan_fail_ms"] = st.last_scan_fail_ms;
  out["scan_ok_count"] = st.scan_ok_count;
  out["scan_fail_count"] = st.scan_fail_count;
}
