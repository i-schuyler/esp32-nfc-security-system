ARTIFACT — product_spec.md (Product Specification v1.0)

# Product Specification (V1)

This is the feature-level spec intended to translate directly into implementation tasks.

## 1) Product Summary

An ESP32-based workshop alarm controlled by NFC:
- NFC wall reader arms/disarms/silences
- Sensor triggers horn + light and logs incidents
- Clearing a triggered alarm writes a short incident summary onto the NFC tag
- Local-only web UI for status, logs, configuration, and OTA

Source: accepted proposal.

## 2) Functional Requirements

### F1 — Control Interfaces (NFC + Web UI)
- Must support arm/disarm/silence via **web UI** when enabled.
- Must support arm/disarm/silence via **NFC** when enabled.
- NFC hardware and tags are **not required** for operation; Setup Wizard allows NFC control to be disabled.
- Web control is configurable; Setup Wizard allows web control to be disabled.
- All control actions are logged.


### F2 — Alarm Behavior
- When ARMED, sensor trigger transitions to TRIGGERED.
- TRIGGERED is latched until cleared.
- TRIGGERED activates horn + light patterns (configurable).
- SILENCED (if enabled) disables horn but preserves alarm memory.
- Output polarity is configurable per channel:
  - `horn_active_low` / `light_active_low` (bool, default false).
  - `active_low=false` means GPIO HIGH = ON, GPIO LOW = OFF.
  - `active_low=true` means GPIO LOW = ON, GPIO HIGH = OFF.
  - Defaults are active-high to match common low-side transistor/MOSFET drivers; outputs still default OFF on boot.
- Light mode selection is a pattern choice:
  - `light_pattern` / `silenced_light_pattern` (enum: off|steady|strobe, default steady).
  - Strobe is a pattern option (not a separate capability toggle); never auto-strobe by default.
- Safety note: inverted polarity can energize a load when it should be OFF; confirm polarity and light mode in bench test before arming.

### F3 — Incident Logging
- Every event is logged per `Event_Log_Schema`.
- An incident summary is written when clearing TRIGGERED.
- Logs must be inspectable and downloadable via UI.

### F4 — NFC Writeback
- On arm: write “armed state” record to tag.
- On clear: overwrite “incident summary” record on tag.
- No secrets are written to tag.

### F5 — Web UI (Local)
- Must include a guided Setup Wizard initiated in the web UI and required until completed.
- Wizard can be restarted by the user (Admin-only) and user can tweak individual settings/sections anytime.

- Must work offline in AP mode.
- Status + recent events visible without admin auth.
- Admin functions require Admin Config Mode gate.

### F6 — Wi‑Fi Behavior
- Joins known Wi‑Fi if available; otherwise hosts password-protected AP.
- User can change SSID/password via UI (Admin gated).
- Wi‑Fi mode changes are logged.

### F7 — OTA Update
- Browser-based `.bin` upload.
- Recoverable and logged; no auto updates.

### F8 — Power Model (Battery Measurement Optional)
- V1 includes battery voltage measurement via ADC divider.
- Battery measurement can be disabled (mains-only installs).
- Thresholds and low-power behaviors are user-configurable via Setup Wizard and config pages.

### F9 — Fail-safe Defaults
- On uncertainty: prevent false-disarm, preserve logs.
- Outputs default OFF on boot until state is known.
- SD missing must be visible and logged; fallback logs exist.

### F9 — Modularity + Scalability
- Add new sensors/outputs without breaking existing contracts.
- Backwards-compatible updates within v1.x (see OTA contract).

## 3) Non-Functional Requirements

- Predictable: no hidden transitions.
- Observable: status, faults, logs inspectable locally.
- Minimal attack surface: NFC + web UI are boundaries.
- Future-you friendly: modular wiring, clear docs.

## 4) Out of Scope (V1)

- Internet notifications
- Cloud dashboards
- Camera integration
- Cryptographic log signing (optional later)

## 5) Acceptance Criteria

Implementation is accepted when:
- Bench Test Checklist passes end-to-end.
- All canonical docs match behavior (no drift).
- UI copy/style approved by you before code generation.


### F10 — Factory Restore
- Admin-only factory restore from the web UI.
- Deletes stored configuration and card allowlist; log wiping is configurable.
- After restore, Setup Wizard is required again.

## 6) Implementation Language
No preference. Choose languages/libraries that minimize risk and maximize maintainability for ESP32 (PlatformIO/Arduino or ESP-IDF are both acceptable as long as contracts remain stable).
