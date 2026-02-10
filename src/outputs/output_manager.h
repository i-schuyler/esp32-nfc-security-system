// src/outputs/output_manager.h
// Role: Deterministic horn/light output control driven by the state machine (M4).
// Notes:
// - No pins are assumed. Unset pins (-1) mean the corresponding output is disabled.
// - Patterns are intentionally conservative in V1. Horn supports "steady" only; light supports
//   "steady" and "strobe". Other patterns fall back to steady with an explicit warning log.

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
  int horn_gpio = -1;
  int light_gpio = -1;
  bool horn_active_low = false;
  bool light_active_low = false;
  bool horn_enabled_cfg = false;
  bool light_enabled_cfg = false;
  String horn_pattern = "steady";
  String light_pattern = "steady";
  String silenced_light_pattern = "steady";
  bool test_active = false;
  bool test_horn_active = false;
  bool test_light_active = false;
  uint32_t test_remaining_s = 0;

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

// Start a non-security output test (duration_ms defaulted by caller).
// Returns false if output is unavailable.
bool wss_outputs_test_start(const char* which, uint32_t duration_ms, String& err);

// Stop any active output test(s) and restore normal state outputs.
void wss_outputs_test_stop(const char* reason);

WssOutputsStatus wss_outputs_status();
