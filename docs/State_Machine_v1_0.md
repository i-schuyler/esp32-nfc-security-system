ARTIFACT — state_machine.md (State Machine v1.0)

# State Machine (V1)

## 0) Control Interfaces (V1)

- NFC and web UI are both supported as control interfaces.
- **NFC is optional**: the system remains operable when NFC hardware is absent or disabled during Setup Wizard.
- **Web UI control is optional**: the operator can disable web control during Setup Wizard.
- When both are enabled, behavior is consistent across interfaces (same transitions, same logs).


This document defines the complete V1 state machine: states, transitions, dominance rules, invariants, and required logs/UI indicators.

## 1) States (Canonical)

- **DISARMED**
- **ARMED**
- **TRIGGERED** (latched until cleared by authorized NFC action)
- **SILENCED** (optional; see semantics below)
- **FAULT** (critical failure; guarantees change)

## 2) Global Invariants (Non‑Negotiable)

- No silent arming/disarming. Every arm/disarm requires explicit NFC action and is logged.
- TRIGGERED is **latched**: it cannot clear on its own.
- Every state transition produces:
  - a log event (with timestamp + reason)
  - a UI update (current state + last transition reason + time)
- On uncertainty, prefer preventing false-disarm and preserving evidence.

## 3) Dominance Rules (When multiple conditions apply)

Priority from highest to lowest:

1) **FAULT**
2) **TRIGGERED**
3) **SILENCED**
4) **ARMED**
5) **DISARMED**

Interpretation:
- If a FAULT is active, system state is FAULT even if ARMED/TRIGGERED would otherwise apply.
- If TRIGGERED occurs, it dominates ARMED and remains until cleared.

## 4) State Semantics

### DISARMED
- Sensors may be monitored for diagnostics.
- Outputs must be off (except optional “steady light on” if configured as a non-alarm utility light; must be explicit in config).
- NFC actions:
- Web UI actions (if enabled): Arm
  - Admin/User (per policy): Arm
  - Admin: Enter Maintenance/Test Mode (UI gating)

### ARMED
- Sensors active (motion and any enabled door/tamper inputs).
- Trigger condition routes to TRIGGERED.
- NFC actions:
- Web UI actions (if enabled): Disarm / Silence
  - Admin/User (per policy): Disarm
  - Admin/User (per policy): Silence (if enabled)
  - Invalid/bad scans: rate-limit + log + lockout as configured

### TRIGGERED (Latched)
- Outputs active in “alarm pattern” (horn + light patterns defined in config).
- NFC actions:
- Web UI actions (if enabled): Clear / Silence
  - Authorized “Clear Alarm” transitions to DISARMED (or ARMED if you choose re-arm-after-clear; **must be decided**).
  - Optional: Silence transitions to SILENCED (see below).
- Clearing TRIGGERED performs:
  - incident summary log
  - NFC incident writeback (fixed-size overwrite)

### SILENCED (Optional)
**Semantics must be explicit. Default V1 semantics:**
- Horn disabled
- Light enabled
- SILENCED duration defaults to **3 minutes** (configurable)
- System retains alarm memory until cleared; SILENCED does not erase evidence

Transitions:
- ARMED → SILENCED (via authorized NFC “Silence”)
- TRIGGERED → SILENCED (via authorized NFC “Silence”)
- SILENCED → TRIGGERED if another trigger occurs (decision: either re-trigger horn or stay silent; must be decided)
- SILENCED → DISARMED via authorized “Disarm” or “Clear” (policy must be explicit)

### FAULT
A FAULT is entered when a critical contract is broken.

Fault sources (examples):
- SD missing (optional: warning vs fault; must be configured)
- RTC missing / invalid time
- NFC reader disconnect
- Sensor bus failure
- Brownout / unstable supply

FAULT behavior must be conservative:
- Must log fault and remain observable
- UI shows fault codes and recommended actions
- Policy must define whether FAULT triggers alarm outputs (e.g., “tamper fault triggers TRIGGERED”) or disables outputs to avoid nuisance.

## 5) Transition Table (Canonical)

Each transition MUST log an event with `event_type`, `from_state`, `to_state`, and `reason_code`.

| From | To | Trigger | Required Conditions | Notes |
|---|---|---|---|---|
| DISARMED | ARMED | NFC: Arm | Authorized card; not locked out | Write “armed” record to NFC (decision: minimal vs full) |
| DISARMED | ARMED | Web UI: Arm | Web control enabled; Admin Config Mode active | Logged; NFC writeback only if NFC enabled |
| ARMED | DISARMED | NFC: Disarm | Authorized card; not locked out | Log who/role; optionally write “disarmed” record |
| ARMED | DISARMED | Web UI: Disarm | Web control enabled; Admin Config Mode active | Logged |
| ARMED | TRIGGERED | Sensor event | Trigger policy satisfied | Latched |
| TRIGGERED | DISARMED | NFC: Clear | **Admin** required (recommended) | Writes incident summary to NFC |
| ARMED/TRIGGERED | SILENCED | NFC: Silence | Authorized per role policy | Does not clear incident |
| ARMED/TRIGGERED | SILENCED | Web UI: Silence | Web control enabled; Admin Config Mode active | Logged |
| ANY | FAULT | Fault detected | Fault policy | FAULT dominates |
| FAULT | prior state | Fault cleared | Manual/explicit | Must not silently resume; requires explicit operator action (recommended) |

## 6) Lockout Sub-state (Orthogonal)

Lockout is **not** a primary state; it is an orthogonal guard condition:
- After N invalid scans within window W → lockout duration L
- While locked out: ignore NFC actions, but continue logging “scan ignored due to lockout”
- UI shows lockout remaining time

## 7) Maintenance/Test Mode (Orthogonal)

Maintenance/Test Mode is an admin-only mode that:
- Temporarily disables alarm outputs from triggers (or routes triggers to logs only) — must be explicit.
- Enables UI controls for output tests and sensor calibration.
- Automatically times out after T minutes (configurable), returning to previous state via explicit log.

(Exact entry gesture is a decision item: NFC hold, NFC scan in UI, etc.)


## 8) NFC Gesture Contract (Tap vs Hold)

- **Tap**: primary gesture for arm/disarm/silence (per role permissions).
- **Hold (recommended 3 seconds)**:
  - enter **Admin Config Mode**
  - confirm **Clear Alarm** (Admin-only)

Hold detection requires the same tag to remain continuously present; partial holds cancel cleanly.


## 9) V1 Default Output Patterns (Configurable)

Defaults (per Decisions):
- TRIGGERED horn: **steady**
- TRIGGERED light: **steady**
- SILENCED: user-configurable; default **3 minutes** of silence window (exact semantics defined in config)

All timing/pattern parameters must be bounded and documented in the Configuration Registry.
