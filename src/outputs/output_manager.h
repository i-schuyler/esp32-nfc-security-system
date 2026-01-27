// src/outputs/output_manager.h
// Role: Deterministic horn/light output control driven by the state machine (M4).
// Notes:
// - No pins are assumed. Unset pins (-1) mean the corresponding output is disabled.
// - Patterns are intentionally conservative in V1. "steady" is implemented; other patterns
//   fall back to steady with an explicit warning log.

#pragma once

#include <Arduino.h>

class WssConfigStore;
class WssEventLogger;

enum class WssOutputLogicalState {
  OFF = 0,
  ON  = 1,
};

struct WssOutputsStatus {
  bool horn_pin_configured = false;
  bool light_pin_configured = false;
  bool horn_enabled_cfg = false;
  bool light_enabled_cfg = false;
  String horn_pattern = "steady";
  String light_pattern = "steady";
  String silenced_light_pattern = "steady";

  // Effective state after applying config + current alarm state.
  bool horn_active = false;
  bool light_active = false;

  // Useful for later bench tests.
  String applied_for_state = "DISARMED";
};

// Initializes output GPIOs (if pins are configured) and forces outputs OFF.
void wss_outputs_begin(WssConfigStore* cfg, WssEventLogger* log);

// Updates any time-based patterns (currently none beyond steady) and applies outputs.
void wss_outputs_loop();

// Applies the output policy for the provided alarm state.
// state_str must be one of: DISARMED|ARMED|TRIGGERED|SILENCED|FAULT.
void wss_outputs_apply_state(const String& state_str);

WssOutputsStatus wss_outputs_status();
