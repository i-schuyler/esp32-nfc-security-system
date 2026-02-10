// src/sensors/sensor_manager.h
// Role: Sensor abstraction layer (M5) that normalizes per-sensor enable/disable, health status,
// and trigger routing into logs + state machine.
//
// Contract anchors:
// - docs/Implementation_Plan_v1_0.md (M5)
// - docs/Event_Log_Schema_v1_0.md (sensor fields)
// - docs/Troubleshooting_v1_0.md (sensor health indicators)

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class WssConfigStore;
class WssEventLogger;

struct WssSensorEntryStatus {
  String sensor_type;          // motion|door|enclosure_open
  String sensor_id;            // stable ID (e.g., motion1, door2)
  bool enabled_cfg = false;    // enabled in config
  bool pin_configured = false; // pin configured (runtime or compile-time)
  String interface;            // e.g., gpio_digital
  String health;               // ok|disabled|unconfigured
  int pin = -1;                // for diagnostics only (no secrets)
  int raw = -1;                // last raw read (0/1)
  bool active = false;         // interpreted active level
  uint32_t last_change_ms = 0;
};

struct WssSensorsStatus {
  bool any_primary_enabled_cfg = false;
  bool any_primary_configured = false;
  String overall; // ok|no_primary_enabled|unconfigured
  String motion_kind; // gpio|ld2410b_uart
  bool ld2410b_selected = false;
  bool ld2410b_enabled_cfg = false;
  bool ld2410b_configured = false;
  String ld2410b_health; // ok|unknown|fault
  uint32_t ld2410b_last_seen_s = 0;
  uint32_t ld2410b_parse_errors = 0;
  int ld2410b_rx_gpio = -1;
  int ld2410b_tx_gpio = -1;
  uint32_t ld2410b_baud = 0;
  bool ld2410b_active = false;
  WssSensorEntryStatus entries[5];
  size_t entry_count = 0;
};

// Initialize sensors. Safe if no pins are configured.
void wss_sensors_begin(WssConfigStore* cfg, WssEventLogger* log);

// Poll sensors and route triggers into logs + state machine.
void wss_sensors_loop();

// Structured status for /api/status.
WssSensorsStatus wss_sensors_status();

// Helper: emit JSON status into an existing JsonObject.
void wss_sensors_write_status_json(JsonObject out);
