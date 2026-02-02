// src/nfc/nfc_manager.cpp
// Role: NFC health + scan logging (M6 slice 0). No auth/state logic.

#include "nfc_manager.h"

#include <Arduino.h>
#include <time.h>

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
static bool g_lockout_active = false;
static uint32_t g_lockout_until_ms = 0;
static uint32_t g_lockout_until_epoch_s = 0;
static uint32_t g_last_lockout_ignored_log_ms = 0;
static const size_t kMaxInvalidScans = 16;
static uint32_t g_invalid_scan_ms[kMaxInvalidScans];
static size_t g_invalid_scan_count = 0;
static size_t g_invalid_scan_head = 0;
static bool g_hold_active = false;
static bool g_hold_ready = false;
static uint32_t g_hold_started_ms = 0;
static uint32_t g_hold_last_seen_ms = 0;
static String g_hold_taghash;
static uint32_t g_last_hold_cancel_log_ms = 0;

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

static void log_hold_event(const char* event_type, const char* reason, const char* role,
                           const String& taghash) {
  if (!g_log) return;
  StaticJsonDocument<192> extra;
  if (reason && reason[0]) extra["reason"] = reason;
  if (role && role[0]) extra["role"] = role;
  if (taghash.length()) extra["tag_prefix"] = taghash.substring(0, 8);
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_info("nfc", event_type, "nfc hold event", &o);
}

static uint32_t cfg_u32(const char* k, uint32_t def) {
  if (!g_cfg) return def;
  JsonObjectConst root = g_cfg->doc().as<JsonObjectConst>();
  if (!root.containsKey(k)) return def;
  if (root[k].is<uint32_t>()) return root[k].as<uint32_t>();
  if (root[k].is<int>()) return (uint32_t)root[k].as<int>();
  return def;
}

static bool time_valid_now() {
  time_t now = time(nullptr);
  return (now > 1700000000);
}

static String iso8601_from_epoch(time_t epoch) {
  struct tm tm_utc;
  gmtime_r(&epoch, &tm_utc);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return String(buf);
}

static void lockout_enter(uint32_t now_ms, uint32_t window_s, uint32_t max_scans, uint32_t duration_s) {
  g_lockout_active = true;
  g_lockout_until_ms = now_ms + duration_s * 1000UL;
  if (time_valid_now()) {
    g_lockout_until_epoch_s = (uint32_t)time(nullptr) + duration_s;
  } else {
    g_lockout_until_epoch_s = 0;
  }
  g_status.lockout_active = true;
  g_status.lockout_remaining_s = duration_s;
  g_status.lockout_until_ts = g_lockout_until_epoch_s ? iso8601_from_epoch(g_lockout_until_epoch_s) : String();
  g_invalid_scan_count = 0;
  g_invalid_scan_head = 0;
  if (g_log) {
    StaticJsonDocument<192> extra;
    extra["window_s"] = window_s;
    extra["max_scans"] = max_scans;
    extra["duration_s"] = duration_s;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_warn("nfc", "lockout_enter", "nfc lockout entered", &o);
  }
}

static void lockout_exit(const char* reason) {
  g_lockout_active = false;
  g_lockout_until_ms = 0;
  g_lockout_until_epoch_s = 0;
  g_status.lockout_active = false;
  g_status.lockout_remaining_s = 0;
  g_status.lockout_until_ts = "";
  g_invalid_scan_count = 0;
  g_invalid_scan_head = 0;
  if (g_log) {
    StaticJsonDocument<128> extra;
    if (reason && reason[0]) extra["reason"] = reason;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_info("nfc", "lockout_exit", "nfc lockout exited", &o);
  }
}

static void lockout_update(uint32_t now_ms) {
  g_status.lockout_active = g_lockout_active;
  if (!g_lockout_active) return;
  if (g_lockout_until_ms != 0 && (int32_t)(g_lockout_until_ms - now_ms) > 0) {
    uint32_t remaining_ms = g_lockout_until_ms - now_ms;
    g_status.lockout_remaining_s = remaining_ms / 1000UL;
    if (g_lockout_until_epoch_s && time_valid_now()) {
      g_status.lockout_until_ts = iso8601_from_epoch(g_lockout_until_epoch_s);
    } else {
      g_status.lockout_until_ts = "";
    }
    return;
  }
  lockout_exit("expired");
}

static void hold_reset(const char* reason, const char* role) {
  if (g_hold_active) {
    uint32_t now_ms = millis();
    if ((uint32_t)(now_ms - g_last_hold_cancel_log_ms) >= 2000) {
      g_last_hold_cancel_log_ms = now_ms;
      log_hold_event("hold_cancel", reason ? reason : "cancel", role, g_hold_taghash);
    }
  }
  g_hold_active = false;
  g_hold_ready = false;
  g_hold_started_ms = 0;
  g_hold_last_seen_ms = 0;
  g_hold_taghash = "";
}

static bool hold_update(const String& taghash, uint32_t now_ms, const char* role) {
  static const uint32_t kHoldMs = 3000;
  static const uint32_t kHoldPresentTimeoutMs = 350;

  if (!g_hold_active || taghash != g_hold_taghash) {
    if (g_hold_active && taghash != g_hold_taghash) {
      hold_reset("tag_changed", role);
    }
    g_hold_active = true;
    g_hold_ready = false;
    g_hold_taghash = taghash;
    g_hold_started_ms = now_ms;
    g_hold_last_seen_ms = now_ms;
    return false;
  }

  g_hold_last_seen_ms = now_ms;
  if (!g_hold_ready && (uint32_t)(now_ms - g_hold_started_ms) >= kHoldMs) {
    g_hold_ready = true;
  }

  return g_hold_ready;
}

static void hold_tick(uint32_t now_ms) {
  if (!g_hold_active) return;
  static const uint32_t kHoldPresentTimeoutMs = 350;
  if ((uint32_t)(now_ms - g_hold_last_seen_ms) > kHoldPresentTimeoutMs) {
    hold_reset("tag_removed", g_status.last_role.c_str());
  }
}

static void invalid_scan_record(uint32_t now_ms, uint32_t window_s, uint32_t max_scans, uint32_t duration_s) {
  if (max_scans == 0 || window_s == 0 || duration_s == 0) return;
  if (max_scans > kMaxInvalidScans) max_scans = kMaxInvalidScans;

  g_invalid_scan_ms[g_invalid_scan_head] = now_ms;
  g_invalid_scan_head = (g_invalid_scan_head + 1) % kMaxInvalidScans;
  if (g_invalid_scan_count < kMaxInvalidScans) g_invalid_scan_count++;

  uint32_t window_ms = window_s * 1000UL;
  uint32_t count = 0;
  for (size_t i = 0; i < g_invalid_scan_count; i++) {
    size_t idx = (g_invalid_scan_head + kMaxInvalidScans - 1 - i) % kMaxInvalidScans;
    uint32_t t = g_invalid_scan_ms[idx];
    if ((uint32_t)(now_ms - t) <= window_ms) {
      count++;
    }
  }

  if (count >= max_scans && !g_lockout_active) {
    lockout_enter(now_ms, window_s, max_scans, duration_s);
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
  g_lockout_active = false;
  g_lockout_until_ms = 0;
  g_lockout_until_epoch_s = 0;
  g_last_lockout_ignored_log_ms = 0;
  g_invalid_scan_count = 0;
  g_invalid_scan_head = 0;
  g_hold_active = false;
  g_hold_ready = false;
  g_hold_started_ms = 0;
  g_hold_last_seen_ms = 0;
  g_hold_taghash = "";
  g_last_hold_cancel_log_ms = 0;

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

  uint32_t now_ms = millis();
  // Slice 0: no reader driver yet. Keep status non-blocking.
  set_health_unavailable();
  lockout_update(now_ms);
  hold_tick(now_ms);

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
  uint32_t now_ms = millis();
  lockout_update(now_ms);

  String taghash = wss_nfc_taghash(uid, uid_len);
  WssNfcRole role = wss_nfc_allowlist_get_role(taghash);
  const char* role_str = wss_nfc_role_to_string(role);
  const char* reason = (role == WSS_NFC_ROLE_UNKNOWN) ? "allowlist_unknown" : "allowlist_match";
  log_scan_ok(role_str, reason, taghash);

  bool hold_ready = hold_update(taghash, now_ms, role_str);

  uint32_t window_s = cfg_u32("invalid_scan_window_s", 30);
  uint32_t max_scans = cfg_u32("invalid_scan_max", 5);
  uint32_t duration_s = cfg_u32("lockout_duration_s", 60);

  if (g_lockout_active) {
    if (role == WSS_NFC_ROLE_ADMIN) {
      lockout_exit("admin_clear");
      if (g_log) {
        StaticJsonDocument<128> extra;
        extra["cleared_by"] = "admin";
        if (taghash.length()) extra["tag_prefix"] = taghash.substring(0, 8);
        JsonObjectConst o = extra.as<JsonObjectConst>();
        g_log->log_info("nfc", "lockout_cleared", "nfc lockout cleared by admin", &o);
      }
    } else {
      if ((uint32_t)(now_ms - g_last_lockout_ignored_log_ms) >= 2000) {
        g_last_lockout_ignored_log_ms = now_ms;
        log_action_event("tap", "ignored", "ignored_due_to_lockout", role_str, taghash);
      }
      return;
    }
  }

  if (hold_ready) {
    WssStateStatus sm = wss_state_status();
    if (role != WSS_NFC_ROLE_ADMIN) {
      log_action_event("clear", "rejected", "not_admin", role_str, taghash);
      hold_reset("not_admin", role_str);
      return;
    }
    if (sm.state != "TRIGGERED") {
      log_action_event("clear", "rejected", "not_triggered", role_str, taghash);
      hold_reset("not_triggered", role_str);
      return;
    }
    bool ok = wss_state_clear("nfc_clear:admin");
    if (ok) {
      log_action_event("clear", "allowed", "ok", role_str, taghash);
    } else {
      log_action_event("clear", "rejected", "state_rejected", role_str, taghash);
    }
    hold_reset("completed", role_str);
    return;
  }

  if (debounced(taghash, now_ms)) return;

  if (role == WSS_NFC_ROLE_UNKNOWN) {
    invalid_scan_record(now_ms, window_s, max_scans, duration_s);
    if (g_lockout_active) return;
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
  uint32_t now_ms = millis();
  lockout_update(now_ms);
  hold_tick(now_ms);
  g_status.hold_active = g_hold_active;
  g_status.hold_ready = g_hold_ready;
  if (g_hold_active && g_hold_started_ms != 0) {
    uint32_t elapsed = (uint32_t)(now_ms - g_hold_started_ms);
    g_status.hold_progress_s = elapsed / 1000UL;
  } else {
    g_status.hold_progress_s = 0;
  }
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
  out["lockout_active"] = st.lockout_active;
  out["lockout_remaining_s"] = st.lockout_remaining_s;
  if (st.lockout_until_ts.length()) out["lockout_until_ts"] = st.lockout_until_ts;
  out["last_scan_ms"] = st.last_scan_ms;
  out["last_scan_ok_ms"] = st.last_scan_ok_ms;
  out["last_scan_fail_ms"] = st.last_scan_fail_ms;
  out["scan_ok_count"] = st.scan_ok_count;
  out["scan_fail_count"] = st.scan_fail_count;
  out["hold_active"] = st.hold_active;
  out["hold_ready"] = st.hold_ready;
  out["hold_progress_s"] = st.hold_progress_s;
}
