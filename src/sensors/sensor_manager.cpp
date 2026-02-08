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

static int cfg_int(const char* k, int def) {
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

static bool pin_input_only(int pin) {
  return pin >= 34 && pin <= 39;
}

static bool uart_pin_ok(int pin, bool is_tx) {
  if (pin < 0) return false;
  if (is_tx && pin_input_only(pin)) return false;
  return true;
}

struct Ld2410bRuntime {
  bool selected = false;
  bool enabled_cfg = false;
  bool configured = false;
  int rx_pin = -1;
  int tx_pin = -1;
  uint32_t baud = 256000;
  bool active = false;
  bool last_active = false;
  bool seen_frame = false;
  uint32_t last_frame_ms = 0;
  uint32_t parse_errors = 0;
  uint32_t last_ok_log_ms = 0;
  uint32_t last_err_log_ms = 0;
  bool serial_started = false;
};

struct Ld2410bParser {
  uint8_t header_idx = 0;
  uint8_t len_idx = 0;
  uint16_t expected_len = 0;
  uint16_t data_idx = 0;
  uint8_t tail_idx = 0;
  bool want_checksum = false;
  uint8_t checksum = 0;
  uint8_t data[64];
};

static Ld2410bRuntime g_ld;
static Ld2410bParser g_ld_parser;

static const uint8_t kLdHeader[] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t kLdTail[] = {0xF8, 0xF7, 0xF6, 0xF5};
static const uint32_t kLdLogIntervalMs = 30000;
static const uint32_t kLdFrameStaleMs = 5000;

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

static void ld2410b_reset_parser() {
  g_ld_parser = Ld2410bParser();
}

static void ld2410b_log_enabled(bool enabled) {
  if (!g_log) return;
  if (enabled) {
    g_log->log_info("sensor", "motion_ld2410b_enabled", "LD2410B motion enabled");
  } else {
    g_log->log_info("sensor", "motion_ld2410b_disabled", "LD2410B motion disabled");
  }
}

static void ld2410b_log_parse_error(uint32_t now_ms, const char* reason) {
  g_ld.parse_errors++;
  if (!g_log) return;
  if (now_ms - g_ld.last_err_log_ms < kLdLogIntervalMs) return;
  g_ld.last_err_log_ms = now_ms;
  StaticJsonDocument<192> extra;
  extra["reason"] = reason;
  extra["parse_errors"] = g_ld.parse_errors;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_warn("sensor", "ld2410b_parse_error", "LD2410B parse error", &o);
}

static bool ld2410b_extract_motion(const uint8_t* data, size_t len, bool& active_out) {
  if (len < 1) return false;
  uint8_t target = data[0];
  if (target == 0x02 && len > 1 && data[1] <= 3) {
    target = data[1];
  } else if (target > 3 && len > 1 && data[1] <= 3) {
    target = data[1];
  } else if (target > 3) {
    return false;
  }
  active_out = (target != 0);
  return true;
}

static void ld2410b_handle_frame(const uint8_t* data, size_t len, uint32_t now_ms) {
  g_ld.seen_frame = true;
  g_ld.last_frame_ms = now_ms;

  bool active = false;
  if (!ld2410b_extract_motion(data, len, active)) {
    ld2410b_log_parse_error(now_ms, "target_state_unknown");
    return;
  }

  if (now_ms - g_ld.last_ok_log_ms >= kLdLogIntervalMs) {
    g_ld.last_ok_log_ms = now_ms;
    if (g_log) {
      StaticJsonDocument<192> extra;
      extra["parse_errors"] = g_ld.parse_errors;
      extra["active"] = active;
      JsonObjectConst o = extra.as<JsonObjectConst>();
      g_log->log_info("sensor", "ld2410b_frame_ok", "LD2410B frame ok", &o);
    }
  }

  g_ld.active = active;
  if (!g_ld.last_active && active) {
    SensorRuntime s;
    s.st.sensor_type = "motion";
    s.st.sensor_id = "ld2410b";
    emit_trigger(s, active ? 1 : 0, active);
  }
  g_ld.last_active = active;
}

static void ld2410b_parse_byte(uint8_t b, uint32_t now_ms) {
  if (g_ld_parser.header_idx < sizeof(kLdHeader)) {
    if (b == kLdHeader[g_ld_parser.header_idx]) {
      g_ld_parser.header_idx++;
      if (g_ld_parser.header_idx == sizeof(kLdHeader)) {
        g_ld_parser.len_idx = 0;
        g_ld_parser.expected_len = 0;
      }
    } else {
      g_ld_parser.header_idx = (b == kLdHeader[0]) ? 1 : 0;
    }
    return;
  }

  if (g_ld_parser.len_idx < 2) {
    if (g_ld_parser.len_idx == 0) {
      g_ld_parser.expected_len = b;
    } else {
      g_ld_parser.expected_len |= (uint16_t)b << 8;
    }
    g_ld_parser.len_idx++;
    if (g_ld_parser.len_idx == 2) {
      if (g_ld_parser.expected_len == 0 || g_ld_parser.expected_len > sizeof(g_ld_parser.data)) {
        ld2410b_log_parse_error(now_ms, "length_invalid");
        ld2410b_reset_parser();
      } else {
        g_ld_parser.data_idx = 0;
        g_ld_parser.tail_idx = 0;
        g_ld_parser.want_checksum = true;
      }
    }
    return;
  }

  if (g_ld_parser.data_idx < g_ld_parser.expected_len) {
    g_ld_parser.data[g_ld_parser.data_idx++] = b;
    return;
  }

  if (g_ld_parser.want_checksum) {
    if (b == kLdTail[0]) {
      g_ld_parser.tail_idx = 1;
      g_ld_parser.want_checksum = false;
      return;
    }
    g_ld_parser.checksum = b;
    g_ld_parser.want_checksum = false;
    return;
  }

  if (g_ld_parser.tail_idx < sizeof(kLdTail)) {
    if (b == kLdTail[g_ld_parser.tail_idx]) {
      g_ld_parser.tail_idx++;
      if (g_ld_parser.tail_idx == sizeof(kLdTail)) {
        ld2410b_handle_frame(g_ld_parser.data, g_ld_parser.expected_len, now_ms);
        ld2410b_reset_parser();
      }
    } else {
      ld2410b_log_parse_error(now_ms, "tail_mismatch");
      ld2410b_reset_parser();
    }
  }
}

static void ld2410b_apply_config() {
  String kind = cfg_str("motion_kind", "gpio");
  bool selected = kind == "ld2410b_uart";
  bool enabled = cfg_bool("motion_enabled", true);
  int rx_pin = cfg_int("motion_ld2410b_rx_gpio", 16);
  int tx_pin = cfg_int("motion_ld2410b_tx_gpio", 17);
  uint32_t baud = (uint32_t)cfg_int("motion_ld2410b_baud", 256000);
  bool configured = selected && enabled && uart_pin_ok(rx_pin, false)
    && uart_pin_ok(tx_pin, true) && rx_pin != tx_pin && baud > 0;

  bool was_enabled = g_ld.selected && g_ld.enabled_cfg;
  bool now_enabled = selected && enabled;
  if (was_enabled != now_enabled) {
    ld2410b_log_enabled(now_enabled);
  }

  bool config_changed = (g_ld.selected != selected)
    || (g_ld.rx_pin != rx_pin) || (g_ld.tx_pin != tx_pin) || (g_ld.baud != baud);

  g_ld.selected = selected;
  g_ld.enabled_cfg = enabled;
  g_ld.rx_pin = rx_pin;
  g_ld.tx_pin = tx_pin;
  g_ld.baud = baud;
  g_ld.configured = configured;

  if (!configured) {
    if (config_changed) {
      g_ld.parse_errors = 0;
      g_ld.last_ok_log_ms = 0;
      g_ld.last_err_log_ms = 0;
    }
    if (g_ld.serial_started) {
      Serial2.end();
      g_ld.serial_started = false;
    }
    if (config_changed) {
      g_ld.active = false;
      g_ld.last_active = false;
      g_ld.seen_frame = false;
      g_ld.last_frame_ms = 0;
      ld2410b_reset_parser();
    }
    return;
  }

  if (!g_ld.serial_started || config_changed) {
    if (g_ld.serial_started) Serial2.end();
    Serial2.begin(g_ld.baud, SERIAL_8N1, g_ld.rx_pin, g_ld.tx_pin);
    g_ld.serial_started = true;
    g_ld.parse_errors = 0;
    g_ld.last_ok_log_ms = 0;
    g_ld.last_err_log_ms = 0;
    g_ld.active = false;
    g_ld.last_active = false;
    g_ld.seen_frame = false;
    g_ld.last_frame_ms = 0;
    ld2410b_reset_parser();
  }
}

static void ld2410b_poll(uint32_t now_ms) {
  if (!g_ld.serial_started) return;
  while (Serial2.available() > 0) {
    int v = Serial2.read();
    if (v < 0) break;
    ld2410b_parse_byte((uint8_t)v, now_ms);
  }
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
  String motion_kind = cfg_str("motion_kind", "gpio");
  bool use_gpio_motion = (motion_kind == "gpio");

  bool motion1 = use_gpio_motion && cfg_bool("motion1_enabled", motion_global);
  bool motion2 = use_gpio_motion && cfg_bool("motion2_enabled", motion_global && false);

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
  ld2410b_apply_config();

  // If config indicates sensors enabled but no pins are configured, emit an explicit warning.
  // This makes "Sensor disconnected" observable without inventing hardware specifics.
  bool any_enabled = false;
  bool any_configured = false;
  for (size_t i = 0; i < g_sensor_count; i++) {
    any_enabled |= g_sensors[i].st.enabled_cfg;
    any_configured |= (g_sensors[i].st.enabled_cfg && g_sensors[i].st.pin_configured);
  }
  bool ld_enabled = g_ld.selected && g_ld.enabled_cfg;
  if (ld_enabled) {
    any_enabled = true;
    if (g_ld.configured) any_configured = true;
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
  auto hash_int = [&](const char* k, int def) {
    int v = cfg_int(k, def);
    h ^= (uint32_t)v;
    h *= 16777619u;
  };
  auto hash_str = [&](const char* k, const char* def) {
    String v = cfg_str(k, def);
    for (size_t i = 0; i < v.length(); i++) {
      h ^= (uint8_t)v[i];
      h *= 16777619u;
    }
  };
  hash_bool("motion_enabled", true);
  hash_bool("door_enabled", false);
  hash_bool("enclosure_open_enabled", false);
  hash_bool("motion1_enabled", true);
  hash_bool("motion2_enabled", false);
  hash_bool("door1_enabled", false);
  hash_bool("door2_enabled", false);
  hash_str("motion_kind", "gpio");
  hash_int("motion_ld2410b_rx_gpio", 16);
  hash_int("motion_ld2410b_tx_gpio", 17);
  hash_int("motion_ld2410b_baud", 256000);

  if (h != last_cfg_hash) {
    last_cfg_hash = h;
    rebuild_sensor_list();
    ld2410b_apply_config();
  }

  uint32_t now_ms = millis();
  ld2410b_poll(now_ms);

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
  uint32_t now_ms = millis();

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

  st.motion_kind = cfg_str("motion_kind", "gpio");
  st.ld2410b_selected = (st.motion_kind == "ld2410b_uart");
  st.ld2410b_enabled_cfg = st.ld2410b_selected && cfg_bool("motion_enabled", true);
  st.ld2410b_configured = g_ld.configured;
  st.ld2410b_parse_errors = g_ld.parse_errors;
  st.ld2410b_rx_gpio = g_ld.rx_pin;
  st.ld2410b_tx_gpio = g_ld.tx_pin;
  st.ld2410b_baud = g_ld.baud;
  st.ld2410b_active = g_ld.active;
  if (g_ld.last_frame_ms > 0 && now_ms >= g_ld.last_frame_ms) {
    st.ld2410b_last_seen_s = (now_ms - g_ld.last_frame_ms) / 1000UL;
  } else {
    st.ld2410b_last_seen_s = 0;
  }
  if (!st.ld2410b_selected || !st.ld2410b_enabled_cfg) {
    st.ld2410b_health = "unknown";
  } else if (!g_ld.configured || !g_ld.seen_frame) {
    st.ld2410b_health = "unknown";
  } else if (now_ms - g_ld.last_frame_ms > kLdFrameStaleMs) {
    st.ld2410b_health = "fault";
  } else {
    st.ld2410b_health = "ok";
  }

  if (st.ld2410b_selected && st.ld2410b_enabled_cfg) {
    any_primary_enabled = true;
    if (st.ld2410b_configured) any_primary_configured = true;
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
  out["motion_kind"] = st.motion_kind;
  {
    JsonObject ld = out.createNestedObject("ld2410b");
    ld["selected"] = st.ld2410b_selected;
    ld["enabled_cfg"] = st.ld2410b_enabled_cfg;
    ld["configured"] = st.ld2410b_configured;
    ld["health"] = st.ld2410b_health;
    ld["last_seen_s"] = st.ld2410b_last_seen_s;
    ld["parse_errors"] = st.ld2410b_parse_errors;
    if (st.ld2410b_rx_gpio >= 0) ld["rx_gpio"] = st.ld2410b_rx_gpio;
    if (st.ld2410b_tx_gpio >= 0) ld["tx_gpio"] = st.ld2410b_tx_gpio;
    if (st.ld2410b_baud > 0) ld["baud"] = st.ld2410b_baud;
    ld["active"] = st.ld2410b_active;
  }

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
