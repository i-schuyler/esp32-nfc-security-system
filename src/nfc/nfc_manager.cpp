// src/nfc/nfc_manager.cpp
// Role: NFC health + scan logging (M6 slice 0). No auth/state logic.

#include "nfc_manager.h"

#include <Arduino.h>

#include "../config/config_store.h"
#include "../logging/event_logger.h"
#include "nfc_allowlist.h"
#include "../state_machine/state_machine.h"

namespace {

static WssConfigStore* g_cfg = nullptr;
static WssEventLogger* g_log = nullptr;
static WssNfcStatus g_status;
static uint32_t g_last_poll_ms = 0;
static bool g_logged_unavailable = false;
static bool g_last_enabled_cfg = true;
static String g_last_taghash;
static uint32_t g_last_tag_ms = 0;
static uint32_t g_last_debounce_log_ms = 0;

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
    g_status.last_scan_result = "ok";
  } else {
    g_status.last_scan_fail_ms = now_ms;
    g_status.scan_fail_count++;
    g_status.last_scan_result = "fail";
    g_status.last_role = "unknown";
  }
  if (reason && reason[0]) {
    g_status.last_scan_reason = reason;
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

static void log_scan_ok(const char* role, const char* reason, const String& taghash) {
  uint32_t now_ms = millis();
  g_status.last_scan_ms = now_ms;
  g_status.last_scan_ok_ms = now_ms;
  g_status.scan_ok_count++;
  g_status.last_scan_result = "ok";
  if (role && role[0]) {
    g_status.last_role = role;
  } else {
    g_status.last_role = "unknown";
  }
  if (reason && reason[0]) {
    g_status.last_scan_reason = reason;
  }
  if (!g_log) return;
  StaticJsonDocument<256> extra;
  extra["result"] = "ok";
  extra["role"] = g_status.last_role;
  if (reason && reason[0]) extra["reason"] = reason;
  if (taghash.length()) extra["tag_prefix"] = taghash.substring(0, 8);
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_info("nfc", "nfc_scan", "nfc scan ok", &o);
}

static void log_action_event(const char* action, const char* outcome, const char* reason,
                             const char* role, const String& taghash) {
  if (!g_log) return;
  StaticJsonDocument<256> extra;
  if (action && action[0]) extra["action"] = action;
  if (outcome && outcome[0]) extra["outcome"] = outcome;
  if (role && role[0]) extra["role"] = role;
  if (reason && reason[0]) extra["reason"] = reason;
  if (taghash.length()) extra["tag_prefix"] = taghash.substring(0, 8);
  JsonObjectConst o = extra.as<JsonObjectConst>();
  if (outcome && strcmp(outcome, "allowed") == 0) {
    g_log->log_info("nfc", "nfc_action", "nfc action allowed", &o);
  } else {
    g_log->log_warn("nfc", "nfc_action", "nfc action rejected", &o);
  }
}

static bool debounced(const String& taghash, uint32_t now_ms) {
  if (taghash.length() == 0) return false;
  static const uint32_t kDebounceMs = 1500;
  if (taghash == g_last_taghash && (uint32_t)(now_ms - g_last_tag_ms) < kDebounceMs) {
    if ((uint32_t)(now_ms - g_last_debounce_log_ms) >= 2000) {
      g_last_debounce_log_ms = now_ms;
      log_action_event("tap", "ignored", "debounced", g_status.last_role.c_str(), taghash);
    }
    return true;
  }
  g_last_taghash = taghash;
  g_last_tag_ms = now_ms;
  return false;
}

} // namespace

void wss_nfc_begin(WssConfigStore* cfg, WssEventLogger* log) {
  g_cfg = cfg;
  g_log = log;
  g_status = WssNfcStatus();
  g_status.feature_enabled = feature_enabled();
  g_status.enabled_cfg = cfg_bool("control_nfc_enabled", true);
  g_status.last_role = "unknown";
  g_status.last_scan_result = "";
  g_status.last_scan_reason = "";
  g_last_enabled_cfg = g_status.enabled_cfg;
  g_last_poll_ms = 0;
  g_logged_unavailable = false;
  g_last_taghash = "";
  g_last_tag_ms = 0;
  g_last_debounce_log_ms = 0;

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

void wss_nfc_on_uid(const uint8_t* uid, size_t uid_len) {
  if (!g_status.feature_enabled || !g_status.enabled_cfg) {
    log_scan_event(false, "nfc_disabled");
    log_action_event("tap", "rejected", "nfc_disabled", "unknown", "");
    return;
  }
  if (!uid || uid_len == 0) {
    log_scan_event(false, "uid_invalid");
    log_action_event("tap", "rejected", "uid_invalid", "unknown", "");
    return;
  }
  String taghash = wss_nfc_taghash(uid, uid_len);
  WssNfcRole role = wss_nfc_allowlist_get_role(taghash);
  const char* role_str = wss_nfc_role_to_string(role);
  const char* reason = (role == WSS_NFC_ROLE_UNKNOWN) ? "allowlist_unknown" : "allowlist_match";
  log_scan_ok(role_str, reason, taghash);

  uint32_t now_ms = millis();
  if (debounced(taghash, now_ms)) return;

  if (role == WSS_NFC_ROLE_UNKNOWN) {
    log_action_event("tap", "rejected", "not_in_allowlist", role_str, taghash);
    return;
  }

  WssStateStatus sm = wss_state_status();
  if (sm.state == "DISARMED") {
    bool allow_user_arm = cfg_bool("allow_user_arm", true);
    bool allowed = (role == WSS_NFC_ROLE_ADMIN) || (role == WSS_NFC_ROLE_USER && allow_user_arm);
    if (!allowed) {
      log_action_event("arm", "rejected", "role_not_permitted", role_str, taghash);
      return;
    }
    bool ok = wss_state_arm((role == WSS_NFC_ROLE_ADMIN) ? "nfc_arm:admin" : "nfc_arm:user");
    if (ok) {
      log_action_event("arm", "allowed", "ok", role_str, taghash);
    } else {
      log_action_event("arm", "rejected", "state_rejected", role_str, taghash);
    }
    return;
  }

  if (sm.state == "ARMED") {
    bool allow_user_disarm = cfg_bool("allow_user_disarm", true);
    bool allowed = (role == WSS_NFC_ROLE_ADMIN) || (role == WSS_NFC_ROLE_USER && allow_user_disarm);
    if (!allowed) {
      log_action_event("disarm", "rejected", "role_not_permitted", role_str, taghash);
      return;
    }
    bool ok = wss_state_disarm((role == WSS_NFC_ROLE_ADMIN) ? "nfc_disarm:admin" : "nfc_disarm:user");
    if (ok) {
      log_action_event("disarm", "allowed", "ok", role_str, taghash);
    } else {
      log_action_event("disarm", "rejected", "state_rejected", role_str, taghash);
    }
    return;
  }

  log_action_event("tap", "rejected", "state_not_supported_in_slice2", role_str, taghash);
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
  out["last_role"] = st.last_role;
  out["last_scan_result"] = st.last_scan_result;
  if (st.last_scan_reason.length()) out["last_scan_reason"] = st.last_scan_reason;
  out["last_scan_ms"] = st.last_scan_ms;
  out["last_scan_ok_ms"] = st.last_scan_ok_ms;
  out["last_scan_fail_ms"] = st.last_scan_fail_ms;
  out["scan_ok_count"] = st.scan_ok_count;
  out["scan_fail_count"] = st.scan_fail_count;
}
