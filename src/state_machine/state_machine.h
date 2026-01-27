// src/state_machine/state_machine.h
// Role: Explicit alarm state machine (M4) with predictable transitions and persistence.
//
// Canonical states:
// - DISARMED
// - ARMED
// - TRIGGERED (latched until cleared)
// - SILENCED (temporary output suppression; returns to pre-silence state)
// - FAULT (dominant indicator; semantics beyond observability are defined by policy)

#pragma once

#include <Arduino.h>

class WssConfigStore;
class WssEventLogger;

enum class WssAlarmState : uint8_t {
  DISARMED = 0,
  ARMED,
  TRIGGERED,
  SILENCED,
  FAULT,
};

struct WssTransitionInfo {
  String ts;
  bool time_valid = false;
  String from;
  String to;
  String reason;
};

struct WssFaultInfo {
  bool active = false;
  String code;   // short machine code (e.g., state_persist_corrupt)
  String detail; // optional short detail (no secrets)
};

struct WssStateStatus {
  String state;
  bool state_machine_active = true;
  WssTransitionInfo last_transition;
  bool silenced = false;
  uint32_t silenced_remaining_s = 0;
  WssFaultInfo fault;
};

// Initialize and load persisted state (defaults to DISARMED if no persisted state is available).
void wss_state_begin(WssConfigStore* cfg, WssEventLogger* log);

// Run periodic checks (e.g., silence expiration) and apply any time-based transitions.
void wss_state_loop();

// Observable status for /api/status.
WssStateStatus wss_state_status();

// Control actions (web/NFC parity):
bool wss_state_arm(const char* reason);
bool wss_state_disarm(const char* reason);
bool wss_state_silence(const char* reason);

// Sensor-driven trigger (wired in later milestones). Safe to call even if sensors are absent.
bool wss_state_trigger(const char* reason);

// Clear a TRIGGERED alarm (NFC admin clear in later milestone; included for completeness).
bool wss_state_clear(const char* reason);

// Fault controls (used sparingly in M4; later milestones will source real faults).
void wss_state_set_fault(const char* code, const char* detail);
void wss_state_clear_fault();
