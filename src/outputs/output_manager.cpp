// src/outputs/output_manager.cpp
// Role: Deterministic horn/light output control driven by the state machine (M4).

#include "output_manager.h"

#include <time.h>

#include "../config/pin_config.h"
#include "../config/config_store.h"
#include "../logging/event_logger.h"

static WssConfigStore* g_cfg = nullptr;
static WssEventLogger* g_log = nullptr;
static WssOutputsStatus g_status;

static bool is_time_valid() {
  time_t now = time(nullptr);
  return (now > 1700000000);
}

static void write_pin(int pin, bool on) {
  if (pin < 0) return;
  digitalWrite(pin, on ? HIGH : LOW);
}

static String norm_pattern(const String& p) {
  if (p.length() == 0) return "steady";
  String s = p;
  s.toLowerCase();
  return s;
}

static String get_cfg_str(const char* key, const char* dflt) {
  if (!g_cfg) return String(dflt);
  JsonObjectConst root = g_cfg->doc().as<JsonObjectConst>();
  if (!root.containsKey(key)) return String(dflt);
  if (root[key].is<const char*>()) return String(root[key].as<const char*>());
  return String(dflt);
}

static bool get_cfg_bool(const char* key, bool dflt) {
  if (!g_cfg) return dflt;
  JsonObjectConst root = g_cfg->doc().as<JsonObjectConst>();
  if (!root.containsKey(key)) return dflt;
  if (root[key].is<bool>()) return root[key].as<bool>();
  return dflt;
}

static void warn_unimplemented_pattern_once(const char* which, const String& p) {
  static bool warned_horn = false;
  static bool warned_light = false;
  static bool warned_sil_light = false;

  bool* slot = nullptr;
  if (String(which) == "horn") slot = &warned_horn;
  else if (String(which) == "light") slot = &warned_light;
  else slot = &warned_sil_light;

  if (*slot) return;
  *slot = true;
  if (!g_log) return;
  StaticJsonDocument<192> extra;
  extra["which"] = which;
  extra["pattern"] = p;
  extra["fallback"] = "steady";
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_warn("outputs", "pattern_unimplemented", "output pattern not implemented; using steady", &o);
}

void wss_outputs_begin(WssConfigStore* cfg, WssEventLogger* log) {
  g_cfg = cfg;
  g_log = log;
  g_status = WssOutputsStatus{};

  g_status.horn_pin_configured = (WSS_PIN_HORN_OUT >= 0);
  g_status.light_pin_configured = (WSS_PIN_LIGHT_OUT >= 0);

  if (WSS_PIN_HORN_OUT >= 0) {
    pinMode(WSS_PIN_HORN_OUT, OUTPUT);
    write_pin(WSS_PIN_HORN_OUT, false);
  }
  if (WSS_PIN_LIGHT_OUT >= 0) {
    pinMode(WSS_PIN_LIGHT_OUT, OUTPUT);
    write_pin(WSS_PIN_LIGHT_OUT, false);
  }

  // Load initial config view for status.
  g_status.horn_enabled_cfg = get_cfg_bool("horn_enabled", true);
  g_status.light_enabled_cfg = get_cfg_bool("light_enabled", true);
  g_status.horn_pattern = norm_pattern(get_cfg_str("horn_pattern", "steady"));
  g_status.light_pattern = norm_pattern(get_cfg_str("light_pattern", "steady"));
  g_status.silenced_light_pattern = norm_pattern(get_cfg_str("silenced_light_pattern", "steady"));

  if (g_log) {
    StaticJsonDocument<256> extra;
    extra["horn_pin_configured"] = g_status.horn_pin_configured;
    extra["light_pin_configured"] = g_status.light_pin_configured;
    extra["horn_enabled"] = g_status.horn_enabled_cfg;
    extra["light_enabled"] = g_status.light_enabled_cfg;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_info("outputs", "outputs_init", "outputs initialized (default OFF)", &o);
  }
}

static void apply_steady_for(int horn_on, int light_on) {
  write_pin(WSS_PIN_HORN_OUT, horn_on != 0);
  write_pin(WSS_PIN_LIGHT_OUT, light_on != 0);
}

void wss_outputs_apply_state(const String& state_str) {
  // Refresh config-derived values each apply (keeps behavior predictable after config patches).
  g_status.horn_enabled_cfg = get_cfg_bool("horn_enabled", true);
  g_status.light_enabled_cfg = get_cfg_bool("light_enabled", true);
  g_status.horn_pattern = norm_pattern(get_cfg_str("horn_pattern", "steady"));
  g_status.light_pattern = norm_pattern(get_cfg_str("light_pattern", "steady"));
  g_status.silenced_light_pattern = norm_pattern(get_cfg_str("silenced_light_pattern", "steady"));

  g_status.applied_for_state = state_str;

  bool horn = false;
  bool light = false;
  String horn_p = "steady";
  String light_p = "steady";

  String s = state_str;
  // Conservative normalization
  // (State strings are canonical; this just avoids surprises.)
  s.toUpperCase();

  if (s == "TRIGGERED") {
    horn = g_status.horn_enabled_cfg && g_status.horn_pin_configured;
    light = g_status.light_enabled_cfg && g_status.light_pin_configured;
    horn_p = g_status.horn_pattern;
    light_p = g_status.light_pattern;
  } else if (s == "SILENCED") {
    horn = false; // explicit contract: horn off in SILENCED
    light = g_status.light_enabled_cfg && g_status.light_pin_configured;
    light_p = g_status.silenced_light_pattern;
  } else {
    // DISARMED, ARMED, FAULT default OFF.
    horn = false;
    light = false;
  }

  // Apply patterns. Only "steady" is implemented in M4.
  if (horn) {
    if (horn_p != "steady") {
      warn_unimplemented_pattern_once("horn", horn_p);
    }
  }
  if (light) {
    if (light_p != "steady") {
      warn_unimplemented_pattern_once((s == "SILENCED") ? "silenced_light" : "light", light_p);
    }
  }

  apply_steady_for(horn ? 1 : 0, light ? 1 : 0);
  g_status.horn_active = horn;
  g_status.light_active = light;
}

void wss_outputs_loop() {
  // Placeholder for time-based patterns (strobe) in later milestones.
  // In M4, outputs are applied whenever state changes via wss_outputs_apply_state().
  (void)is_time_valid();
}

WssOutputsStatus wss_outputs_status() {
  return g_status;
}
