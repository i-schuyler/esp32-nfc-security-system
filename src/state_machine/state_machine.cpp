// src/state_machine/state_machine.cpp
// Role: Explicit alarm state machine (M4) with predictable transitions and persistence.

#include "state_machine.h"

#include <Preferences.h>
#include <time.h>

#include "../config/config_store.h"
#include "../logging/event_logger.h"

static const char* kPrefsNs = "wss_state";
static const char* kPrefsKeyState = "state";
static const char* kPrefsKeyPreSilence = "pre_sil";
static const char* kPrefsKeySilenceUntil = "sil_until";

static WssConfigStore* g_cfg = nullptr;
static WssEventLogger* g_log = nullptr;

static WssAlarmState g_state = WssAlarmState::DISARMED;
static WssAlarmState g_pre_silence = WssAlarmState::TRIGGERED;
static uint32_t g_silenced_until_epoch_s = 0; // 0 means not persistable (time invalid)
static uint32_t g_silence_started_ms = 0;

static WssTransitionInfo g_last;
static WssFaultInfo g_fault;

static bool time_valid_now() {
  time_t now = time(nullptr);
  return (now > 1700000000);
}

static String iso8601_now(bool& tv) {
  time_t now = time(nullptr);
  tv = time_valid_now();
  struct tm tm_utc;
  gmtime_r(&now, &tm_utc);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return String(buf);
}

static const char* to_str(WssAlarmState s) {
  switch (s) {
    case WssAlarmState::DISARMED: return "DISARMED";
    case WssAlarmState::ARMED: return "ARMED";
    case WssAlarmState::TRIGGERED: return "TRIGGERED";
    case WssAlarmState::SILENCED: return "SILENCED";
    case WssAlarmState::FAULT: return "FAULT";
  }
  return "DISARMED";
}

static bool parse_state(const String& s, WssAlarmState& out) {
  String u = s;
  u.toUpperCase();
  if (u == "DISARMED") { out = WssAlarmState::DISARMED; return true; }
  if (u == "ARMED") { out = WssAlarmState::ARMED; return true; }
  if (u == "TRIGGERED") { out = WssAlarmState::TRIGGERED; return true; }
  if (u == "SILENCED") { out = WssAlarmState::SILENCED; return true; }
  if (u == "FAULT") { out = WssAlarmState::FAULT; return true; }
  return false;
}

static uint32_t cfg_u32(const char* key, uint32_t dflt) {
  if (!g_cfg) return dflt;
  JsonObject root = g_cfg->doc().as<JsonObject>();
  if (!root.containsKey(key)) return dflt;
  if (root[key].is<uint32_t>()) return root[key].as<uint32_t>();
  if (root[key].is<int>()) return (uint32_t)root[key].as<int>();
  return dflt;
}

static bool persist_state() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNs, false)) return false;
  prefs.putString(kPrefsKeyState, to_str(g_state));
  prefs.putString(kPrefsKeyPreSilence, to_str(g_pre_silence));
  prefs.putULong(kPrefsKeySilenceUntil, (unsigned long)g_silenced_until_epoch_s);
  prefs.end();
  return true;
}

static void log_transition(WssAlarmState from, WssAlarmState to, const char* reason) {
  bool tv = false;
  String ts = iso8601_now(tv);
  g_last.ts = ts;
  g_last.time_valid = tv;
  g_last.from = to_str(from);
  g_last.to = to_str(to);
  g_last.reason = reason ? String(reason) : String("unspecified");

  if (!g_log) return;
  StaticJsonDocument<256> extra;
  extra["from"] = g_last.from;
  extra["to"] = g_last.to;
  extra["reason"] = g_last.reason;
  extra["state"] = g_last.to;
  if (!tv) extra["time_valid"] = false;
  if (g_state == WssAlarmState::SILENCED) {
    extra["silenced_until_epoch_s"] = (uint32_t)g_silenced_until_epoch_s;
  }
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_info("state", "state_transition", "state transition", &o);
}

static bool transition_to(WssAlarmState next, const char* reason) {
  if (g_fault.active) {
    // In M4, FAULT is treated as dominant for observability. Control actions do not clear faults.
    // If already in FAULT, allow transitions only within FAULT (no-op).
    if (g_state == WssAlarmState::FAULT) return false;
  }

  if (next == g_state) return false;
  WssAlarmState prev = g_state;
  g_state = next;
  (void)persist_state();
  log_transition(prev, next, reason);
  return true;
}

static bool ensure_fault_state_if_needed() {
  if (!g_fault.active) return false;
  if (g_state == WssAlarmState::FAULT) return false;
  WssAlarmState prev = g_state;
  g_state = WssAlarmState::FAULT;
  (void)persist_state();
  log_transition(prev, g_state, "fault_entered");
  return true;
}

void wss_state_begin(WssConfigStore* cfg, WssEventLogger* log) {
  g_cfg = cfg;
  g_log = log;
  g_fault = WssFaultInfo{};
  g_last = WssTransitionInfo{};

  // Load persisted state.
  Preferences prefs;
  bool ok = prefs.begin(kPrefsNs, true);
  if (ok) {
    String s = prefs.getString(kPrefsKeyState, "DISARMED");
    String ps = prefs.getString(kPrefsKeyPreSilence, "TRIGGERED");
    g_silenced_until_epoch_s = (uint32_t)prefs.getULong(kPrefsKeySilenceUntil, 0);
    prefs.end();

    WssAlarmState parsed = WssAlarmState::DISARMED;
    WssAlarmState parsed_ps = WssAlarmState::TRIGGERED;
    bool p1 = parse_state(s, parsed);
    bool p2 = parse_state(ps, parsed_ps);

    if (!p1 || !p2) {
      // Corrupt persisted state: enter FAULT but do not silently disarm.
      g_fault.active = true;
      g_fault.code = "state_persist_corrupt";
      g_fault.detail = "invalid persisted state";
      g_state = WssAlarmState::FAULT;
      g_pre_silence = WssAlarmState::TRIGGERED;
      g_silenced_until_epoch_s = 0;
      if (g_log) {
        StaticJsonDocument<256> extra;
        extra["persisted_state"] = s;
        extra["persisted_pre_silence"] = ps;
        JsonObjectConst o = extra.as<JsonObjectConst>();
        g_log->log_error("state", "state_persist_corrupt", "persisted state corrupt; entering FAULT", &o);
      }
      (void)persist_state();
      return;
    }

    g_state = parsed;
    g_pre_silence = parsed_ps;
  } else {
    // No persistence available; default DISARMED.
    g_state = WssAlarmState::DISARMED;
    g_pre_silence = WssAlarmState::TRIGGERED;
    g_silenced_until_epoch_s = 0;
    if (g_log) g_log->log_warn("state", "state_persist_unavailable", "state persistence unavailable; default DISARMED");
  }

  // If we booted into SILENCED but time is invalid / timer not persisted, expire immediately.
  if (g_state == WssAlarmState::SILENCED) {
    if (g_silenced_until_epoch_s == 0 || !time_valid_now()) {
      if (g_log) g_log->log_warn("state", "silence_not_persisted", "silence timer not persisted; expiring silence on boot");
      g_state = g_pre_silence;
      g_silenced_until_epoch_s = 0;
      (void)persist_state();
    }
  }

  // Record a synthetic "boot_state" transition for observability.
  log_transition(g_state, g_state, "boot_state");
}

void wss_state_loop() {
  (void)ensure_fault_state_if_needed();

  if (g_state != WssAlarmState::SILENCED) return;

  uint32_t remaining_s = 0;
  if (g_silenced_until_epoch_s != 0 && time_valid_now()) {
    uint32_t now_s = (uint32_t)time(nullptr);
    if (now_s >= g_silenced_until_epoch_s) {
      // Return to pre-silence state.
      WssAlarmState prev = g_state;
      g_state = g_pre_silence;
      g_silenced_until_epoch_s = 0;
      (void)persist_state();
      log_transition(prev, g_state, "silence_expired");
      return;
    }
    remaining_s = g_silenced_until_epoch_s - now_s;
  } else {
    // Time invalid: use a monotonic timer (not persisted across reboot).
    uint32_t dur_ms = cfg_u32("silenced_duration_s", 180) * 1000UL;
    if (dur_ms == 0) dur_ms = 180000UL;
    if (g_silence_started_ms == 0) g_silence_started_ms = millis();
    uint32_t elapsed = (uint32_t)(millis() - g_silence_started_ms);
    if (elapsed >= dur_ms) {
      WssAlarmState prev = g_state;
      g_state = g_pre_silence;
      log_transition(prev, g_state, "silence_expired");
      return;
    }
    remaining_s = (dur_ms - elapsed) / 1000UL;
  }

  // Keep last transition info intact; remaining is reported via status.
  (void)remaining_s;
}

WssStateStatus wss_state_status() {
  WssStateStatus st;
  st.state = to_str(g_fault.active ? WssAlarmState::FAULT : g_state);
  st.state_machine_active = true;
  st.last_transition = g_last;
  st.fault = g_fault;
  st.silenced = (g_state == WssAlarmState::SILENCED);

  if (g_state == WssAlarmState::SILENCED) {
    uint32_t remaining_s = 0;
    if (g_silenced_until_epoch_s != 0 && time_valid_now()) {
      uint32_t now_s = (uint32_t)time(nullptr);
      remaining_s = (now_s >= g_silenced_until_epoch_s) ? 0 : (g_silenced_until_epoch_s - now_s);
    } else {
      uint32_t dur_ms = cfg_u32("silenced_duration_s", 180) * 1000UL;
      uint32_t elapsed = (g_silence_started_ms == 0) ? 0 : (uint32_t)(millis() - g_silence_started_ms);
      remaining_s = (elapsed >= dur_ms) ? 0 : ((dur_ms - elapsed) / 1000UL);
    }
    st.silenced_remaining_s = remaining_s;
  }
  return st;
}

bool wss_state_arm(const char* reason) {
  if (g_fault.active) return false;
  // M5: "armed correctness" requires at least one primary sensor enabled.
  // (Defined at the configuration level; physical pin-map may be TBD.)
  {
    bool motion_global = (g_cfg ? (g_cfg->doc()["motion_enabled"] | true) : true);
    bool door_global = (g_cfg ? (g_cfg->doc()["door_enabled"] | false) : false);
    bool motion1 = (g_cfg ? (g_cfg->doc()["motion1_enabled"] | motion_global) : motion_global);
    bool motion2 = (g_cfg ? (g_cfg->doc()["motion2_enabled"] | false) : false);
    bool door1 = (g_cfg ? (g_cfg->doc()["door1_enabled"] | door_global) : door_global);
    bool door2 = (g_cfg ? (g_cfg->doc()["door2_enabled"] | false) : false);
    bool any_primary = (motion1 || motion2 || door1 || door2);
    if (!any_primary) {
      if (g_log) g_log->log_warn("state", "arm_blocked", "arm blocked: no primary sensor enabled");
      return false;
    }
  }
  if (g_state != WssAlarmState::DISARMED) {
    if (g_log) g_log->log_warn("state", "invalid_transition", String("arm from ") + to_str(g_state));
    return false;
  }
  return transition_to(WssAlarmState::ARMED, reason ? reason : "web_arm");
}

bool wss_state_disarm(const char* reason) {
  if (g_fault.active) return false;
  if (g_state == WssAlarmState::ARMED || g_state == WssAlarmState::SILENCED) {
    // Disarm from SILENCED returns to DISARMED (explicitly ends the session).
    g_silenced_until_epoch_s = 0;
    g_silence_started_ms = 0;
    return transition_to(WssAlarmState::DISARMED, reason ? reason : "web_disarm");
  }
  if (g_log) g_log->log_warn("state", "invalid_transition", String("disarm from ") + to_str(g_state));
  return false;
}

bool wss_state_silence(const char* reason) {
  if (g_fault.active) return false;

  if (!(g_state == WssAlarmState::TRIGGERED || g_state == WssAlarmState::ARMED)) {
    if (g_log) g_log->log_warn("state", "invalid_transition", String("silence from ") + to_str(g_state));
    return false;
  }

  g_pre_silence = g_state;
  g_silence_started_ms = millis();

  uint32_t dur_s = cfg_u32("silenced_duration_s", 180);
  if (dur_s == 0) dur_s = 180;
  if (time_valid_now()) {
    g_silenced_until_epoch_s = (uint32_t)time(nullptr) + dur_s;
  } else {
    g_silenced_until_epoch_s = 0;
    if (g_log) g_log->log_warn("state", "silence_time_invalid", "time invalid; silence timer not persisted across reboot");
  }

  (void)persist_state();
  return transition_to(WssAlarmState::SILENCED, reason ? reason : "web_silence");
}

bool wss_state_trigger(const char* reason) {
  if (g_fault.active) return false;

  if (g_state == WssAlarmState::ARMED || g_state == WssAlarmState::SILENCED) {
    // If silenced, triggering re-enables TRIGGERED outputs.
    g_silenced_until_epoch_s = 0;
    g_silence_started_ms = 0;
    return transition_to(WssAlarmState::TRIGGERED, reason ? reason : "sensor_trigger");
  }
  if (g_state == WssAlarmState::DISARMED) {
    // No transition, but log for evidence.
    if (g_log) g_log->log_info("state", "trigger_ignored", "trigger ignored while DISARMED");
    return false;
  }
  // Already TRIGGERED.
  return false;
}

bool wss_state_clear(const char* reason) {
  if (g_fault.active) return false;
  if (g_state != WssAlarmState::TRIGGERED) {
    if (g_log) g_log->log_warn("state", "invalid_transition", String("clear from ") + to_str(g_state));
    return false;
  }
  g_silenced_until_epoch_s = 0;
  g_silence_started_ms = 0;
  // Conservative default per contract: clear returns to DISARMED.
  return transition_to(WssAlarmState::DISARMED, reason ? reason : "clear");
}

void wss_state_set_fault(const char* code, const char* detail) {
  g_fault.active = true;
  g_fault.code = code ? String(code) : String("fault");
  g_fault.detail = detail ? String(detail) : String("");
  (void)ensure_fault_state_if_needed();
  if (g_log) {
    StaticJsonDocument<192> extra;
    extra["fault_code"] = g_fault.code;
    if (g_fault.detail.length()) extra["fault_detail"] = g_fault.detail;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_error("state", "fault_active", "fault active", &o);
  }
}

void wss_state_clear_fault() {
  if (!g_fault.active) return;
  if (g_log) g_log->log_warn("state", "fault_cleared", "fault cleared (operator action required)");
  g_fault = WssFaultInfo{};
  // Do not auto-restore prior state; conservative choice is DISARMED.
  g_state = WssAlarmState::DISARMED;
  (void)persist_state();
  log_transition(WssAlarmState::FAULT, g_state, "fault_cleared");
}
