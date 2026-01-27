// src/sensors/sensor_manager.cpp
// Role: Sensor abstraction layer (M5) that normalizes per-sensor enable/disable, health status,
// and trigger routing into logs + state machine.

#include "sensor_manager.h"

#include <Arduino.h>

#include "../config/config_store.h"
#include "../config/pin_config.h"
#include "../logging/event_logger.h"
#include "../state_machine/state_machine.h"

// Minimal digital sensor model (GPIO). Other sensor interfaces (e.g., UART LD2410)
// are intentionally not assumed until wiring and contracts exist.

namespace {

struct SensorRuntime {
  WssSensorEntryStatus st;
  bool seen_active = false;
  bool last_active = false;
  bool last_raw_valid = false;
  bool warned_unconfigured = false;
};

static WssConfigStore* g_cfg = nullptr;
static WssEventLogger* g_log = nullptr;

static SensorRuntime g_sensors[5];
static size_t g_sensor_count = 0;

static bool cfg_bool(const char* k, bool def) {
  if (!g_cfg) return def;
  return g_cfg->doc()[k] | def;
}

static String cfg_str(const char* k, const char* def) {
  if (!g_cfg) return String(def);
  return String(g_cfg->doc()[k] | def);
}

static bool pin_configured(int pin) {
  return pin >= 0;
}

static void log_init_status(const SensorRuntime& s) {
  if (!g_log) return;
  StaticJsonDocument<256> extra;
  extra["sensor_type"] = s.st.sensor_type;
  extra["sensor_id"] = s.st.sensor_id;
  extra["enabled_cfg"] = s.st.enabled_cfg;
  extra["pin_configured"] = s.st.pin_configured;
  if (s.st.pin_configured) extra["pin"] = s.st.pin;
  extra["health"] = s.st.health;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_info("sensor", "sensor_init", "sensor init", &o);
}

static void configure_pin_if_needed(SensorRuntime& s) {
  if (!s.st.pin_configured) return;

  // Pull mode is configurable; defaults are chosen to reduce surprise.
  // Allowed values: pullup|pulldown|floating
  String pull = cfg_str((String(s.st.sensor_id) + "_pull").c_str(), "pullup");
  if (pull == "pulldown") {
#if defined(INPUT_PULLDOWN)
    pinMode(s.st.pin, INPUT_PULLDOWN);
#else
    pinMode(s.st.pin, INPUT);
#endif
  } else if (pull == "floating") {
    pinMode(s.st.pin, INPUT);
  } else {
    // default pullup
    pinMode(s.st.pin, INPUT_PULLUP);
  }
}

static bool interpret_active(const SensorRuntime& s, int raw) {
  // Allowed values: high|low
  String active_level = cfg_str((String(s.st.sensor_id) + "_active_level").c_str(), "high");
  if (active_level == "low") {
    return raw == LOW;
  }
  return raw == HIGH;
}

static void emit_trigger(SensorRuntime& s, int raw, bool active) {
  if (!g_log) return;
  StaticJsonDocument<256> extra;
  extra["sensor_type"] = s.st.sensor_type;
  extra["sensor_id"] = s.st.sensor_id;
  extra["raw"] = raw;
  extra["active"] = active;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_warn("sensor", "sensor_trigger", "sensor trigger", &o);

  // Route into state machine with an explicit reason string.
  String reason = String("sensor:") + s.st.sensor_type + ":" + s.st.sensor_id;
  (void)wss_state_trigger(reason.c_str());
}

static void add_sensor(const char* type, const char* id, int pin, bool enabled_cfg) {
  if (g_sensor_count >= (sizeof(g_sensors) / sizeof(g_sensors[0]))) return;
  SensorRuntime& s = g_sensors[g_sensor_count++];
  s = SensorRuntime();
  s.st.sensor_type = type;
  s.st.sensor_id = id;
  s.st.enabled_cfg = enabled_cfg;
  s.st.pin = pin;
  s.st.pin_configured = pin_configured(pin);
  s.st.interface = "gpio_digital";

  if (!enabled_cfg) {
    s.st.health = "disabled";
  } else if (!s.st.pin_configured) {
    s.st.health = "unconfigured";
  } else {
    s.st.health = "ok";
  }

  configure_pin_if_needed(s);
  log_init_status(s);
}

static void rebuild_sensor_list() {
  g_sensor_count = 0;

  // Per-sensor enable keys (M5) with legacy fallbacks (motion_enabled, door_enabled).
  bool motion_global = cfg_bool("motion_enabled", true);
  bool door_global = cfg_bool("door_enabled", false);

  bool motion1 = cfg_bool("motion1_enabled", motion_global);
  bool motion2 = cfg_bool("motion2_enabled", motion_global && false);

  bool door1 = cfg_bool("door1_enabled", door_global);
  bool door2 = cfg_bool("door2_enabled", door_global && false);

  bool enclosure = cfg_bool("enclosure_open_enabled", false);

  add_sensor("motion", "motion1", WSS_PIN_MOTION_1, motion1);
  add_sensor("motion", "motion2", WSS_PIN_MOTION_2, motion2);
  add_sensor("door", "door1", WSS_PIN_DOOR_1, door1);
  add_sensor("door", "door2", WSS_PIN_DOOR_2, door2);
  add_sensor("enclosure_open", "enclosure1", WSS_PIN_ENCLOSURE_OPEN, enclosure);
}

} // namespace

void wss_sensors_begin(WssConfigStore* cfg, WssEventLogger* log) {
  g_cfg = cfg;
  g_log = log;
  rebuild_sensor_list();

  // If config indicates sensors enabled but no pins are configured, emit an explicit warning.
  // This makes "Sensor disconnected" observable without inventing hardware specifics.
  bool any_enabled = false;
  bool any_configured = false;
  for (size_t i = 0; i < g_sensor_count; i++) {
    any_enabled |= g_sensors[i].st.enabled_cfg;
    any_configured |= (g_sensors[i].st.enabled_cfg && g_sensors[i].st.pin_configured);
  }
  if (any_enabled && !any_configured && g_log) {
    g_log->log_warn("sensor", "sensor_unconfigured", "sensors enabled but no sensor pins configured");
  }
}

void wss_sensors_loop() {
  // Config can be updated at runtime; rebuild list if any enable key changes.
  // In M5, keep this simple and low-risk by polling a small set of keys.
  static uint32_t last_cfg_hash = 0;
  uint32_t h = 2166136261u;
  auto hash_bool = [&](const char* k, bool def) {
    bool v = cfg_bool(k, def);
    h ^= (uint32_t)(v ? 1 : 0);
    h *= 16777619u;
  };
  hash_bool("motion_enabled", true);
  hash_bool("door_enabled", false);
  hash_bool("enclosure_open_enabled", false);
  hash_bool("motion1_enabled", true);
  hash_bool("motion2_enabled", false);
  hash_bool("door1_enabled", false);
  hash_bool("door2_enabled", false);

  if (h != last_cfg_hash) {
    last_cfg_hash = h;
    rebuild_sensor_list();
  }

  for (size_t i = 0; i < g_sensor_count; i++) {
    SensorRuntime& s = g_sensors[i];

    // Disabled sensors are still observable but do not trigger.
    if (!s.st.enabled_cfg) continue;

    if (!s.st.pin_configured) {
      // Explicitly warn once per sensor.
      if (!s.warned_unconfigured && g_log) {
        s.warned_unconfigured = true;
        StaticJsonDocument<192> extra;
        extra["sensor_type"] = s.st.sensor_type;
        extra["sensor_id"] = s.st.sensor_id;
        JsonObjectConst o = extra.as<JsonObjectConst>();
        g_log->log_warn("sensor", "sensor_pin_unset", "sensor enabled but pin is unset", &o);
      }
      continue;
    }

    int raw = digitalRead(s.st.pin);
    s.st.raw = raw;
    bool active = interpret_active(s, raw);
    s.st.active = active;

    uint32_t now_ms = millis();
    if (!s.last_raw_valid) {
      s.last_raw_valid = true;
      s.last_active = active;
      s.st.last_change_ms = now_ms;
      continue;
    }

    if (active != s.last_active) {
      s.last_active = active;
      s.st.last_change_ms = now_ms;

      // Trigger on rising edge into active.
      if (active) {
        emit_trigger(s, raw, active);
      }
    }
  }
}

WssSensorsStatus wss_sensors_status() {
  WssSensorsStatus st;
  st.entry_count = 0;

  bool any_primary_enabled = false;
  bool any_primary_configured = false;

  for (size_t i = 0; i < g_sensor_count; i++) {
    const SensorRuntime& s = g_sensors[i];
    if (st.entry_count < (sizeof(st.entries) / sizeof(st.entries[0]))) {
      st.entries[st.entry_count++] = s.st;
    }
    bool is_primary = (s.st.sensor_type == "motion" || s.st.sensor_type == "door");
    if (is_primary && s.st.enabled_cfg) {
      any_primary_enabled = true;
      if (s.st.pin_configured) any_primary_configured = true;
    }
  }

  st.any_primary_enabled_cfg = any_primary_enabled;
  st.any_primary_configured = any_primary_configured;

  if (!any_primary_enabled) {
    st.overall = "no_primary_enabled";
  } else if (!any_primary_configured) {
    st.overall = "unconfigured";
  } else {
    st.overall = "ok";
  }

  return st;
}

void wss_sensors_write_status_json(JsonObject out) {
  WssSensorsStatus st = wss_sensors_status();

  out["overall"] = st.overall;
  out["any_primary_enabled_cfg"] = st.any_primary_enabled_cfg;
  out["any_primary_configured"] = st.any_primary_configured;

  JsonArray arr = out.createNestedArray("sensors");
  for (size_t i = 0; i < st.entry_count; i++) {
    const WssSensorEntryStatus& e = st.entries[i];
    JsonObject o = arr.createNestedObject();
    o["type"] = e.sensor_type;
    o["id"] = e.sensor_id;
    o["enabled_cfg"] = e.enabled_cfg;
    o["pin_configured"] = e.pin_configured;
    o["interface"] = e.interface;
    o["health"] = e.health;
    if (e.pin_configured) o["pin"] = e.pin;
    if (e.raw >= 0) o["raw"] = e.raw;
    o["active"] = e.active;
    o["last_change_ms"] = (uint32_t)e.last_change_ms;
  }
}
