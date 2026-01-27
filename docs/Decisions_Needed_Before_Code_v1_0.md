ARTIFACT — decisions_needed.md (Decisions Needed Before Code v1.0)

# Decisions Needed Before Code Generation

This file lists the **remaining unknowns** that must be explicitly decided to avoid hidden assumptions.

If any item remains undecided, firmware work should stop and the decision should be recorded here.

## A) NFC Tags (Type + Capacity)
The system will **support multiple Type 2 tag capacities**.

**Decision:** Support both small and large tags by detecting capacity and using a minimal/truncated record when needed.

- Device detects available NDEF capacity at write time.
- If capacity is insufficient for the preferred “full” record, device writes a **minimal record** (defined in `NFC_Data_Contracts_v1_0.md`) and logs `nfc_write_truncated=true`.
- The incident and armed records are **cleared and overwritten** each time new information is written (no incremental append).
- Optional (nice-to-have V1): include a **URL NDEF record** so a phone tap opens the local web UI. If enabled, the device rewrites the entire NDEF message each time, preserving the URL record alongside the current system record when capacity allows.



**Decision:** System NDEF record type (External Type):
- `urn:nfc:ext:esp32-nfc-security-system:v1`

**Decision:** Mandatory on-tag fields (at minimum) are fixed in the NFC contract:
- Always: version, record type, device identifier
- Armed: timestamp + state
- Incident: trigger timestamp, clear timestamp, trigger source, cleared-by role

**Decision:** Card provisioning is supported in V1 (explicit mode only; no “magic learning”).

## B) NFC Authentication Model
Canonical reminder: **UID is not secret**; do not treat UID alone as authentication.

**Decision:** Choose **Option 3** for V1:
- Device-stored allowlist (hashed tag identifier) + roles (Admin/User)
- Strong rate-limiting + lockouts + logging
- Sensitive actions gated by Admin Config Mode + (recommended) hold-to-confirm

Rationale: most reliable with Type 2 tags, lowest surprise, easiest to scale, and preserves an upgrade path to stronger cryptographic tags later.

## C) “Long Tap” / Hold-to-Confirm NFC Gesture
You asked if the reader can detect long taps.

**Decision:** YES — implement a “hold to confirm” gesture.

- Require same tag continuously present for **HOLD_SECONDS** (recommended 3s) to:
  - enter **Admin Config Mode**
  - confirm **Clear Alarm** (Admin-only)
- Normal tap remains the primary gesture for arm/disarm/silence (per role policy).
- UI must show hold progress and log hold start/complete/cancel.

## C1) Admin Config Mode Entry When NFC Is Absent/Disabled

**Decision:** Admin Config Mode entry must work without NFC. In V1, Admin Config Mode can be entered via a web-based admin password set during the Setup Wizard.

Contract:
- The password itself is never logged and is stored as a one-way hash.
- Entering Admin Config Mode issues a short-lived session token; sensitive API routes require that token.
- NFC “hold to confirm” remains supported (and preferred) when NFC is present, but is not required for Admin Config Mode entry.

## D) Wi‑Fi SSID Defaults + Rename Policy

**Decision:** Default SSID format is:  
`Workshop Security System - A1B2` (A1B2 derived from ESP32 MAC last bytes).

Contract:
- The SSID suffix exists to prevent collisions when multiple devices are nearby.
- The SSID and password can be changed via web UI, but only while Admin Config Mode is active.
- All Wi‑Fi credential changes are logged as `config_change` events without storing secret values in logs.

## E) Power Model Scope (V1)

**Decision:** V1 includes explicit battery voltage measurement hardware (ADC divider).

Contract:
- Battery measurement can be **enabled/disabled** by the operator (opt‑out supported).
- Setup Wizard includes:
  - power source selection (mains-only vs battery-backed)
  - nominal system voltage selection (e.g., 12V / 24V; exact options defined in config)
  - battery measurement enable/disable toggle
  - user-configurable thresholds and behaviors

User-configurable behaviors (when measurement is enabled):
- when to disable Wi‑Fi (STA/AP)
- when to enter low-power mode / sleep
- how often to wake for status updates

Logging:
- power events are logged, including battery voltage (when enabled) and any transition into low‑power behavior.

## F) Sensor Set for V1

**Decision:** V1 requires at least **one** primary sensor for correct operation:
- **x1 motion sensor OR x1 door/window sensor**.

The operator can configure additional sensors in the web UI.

Minimum supported sensor count (V1 capability target):
- Motion sensors: up to **2**
- Door/window sensors: up to **2**
- Enclosure open sensor: **1** (tamper)

Contract:
- Each sensor input can be enabled/disabled independently in configuration.
- Trigger policy is explicit per sensor type (motion vs door/window vs enclosure tamper).
- Sensor presence/faults are observable in UI and logged.

## G) Outputs & Patterns

**Decision:** Alarm patterns are explicit and default to steady outputs.

- Horn pattern in TRIGGERED: **steady**
- Light pattern in TRIGGERED: **steady** (default)

SILENCED semantics (defaults; configurable in UI):
- Horn: **OFF** (default)
- Light: **ON** (default)
- Duration: **3 minutes** (default), after which the system returns to the pre-silence behavior defined by policy.

Contract:
- Pattern timing and SILENCED behavior are configuration-backed and visible in UI.
- All pattern changes and SILENCED actions are logged.

## H) SD Card + Filesystem Support

**Decision:** Prefer support for both FAT32 and exFAT when feasible.

Contract:
- If the firmware build includes exFAT support (e.g., via SdFat exFAT), the device supports:
  - FAT32 (8–32GB typical)
  - exFAT (>32GB common)
- The device detects filesystem type at mount time and logs the detected type.
- If exFAT support is not included in the firmware build, V1 support is limited to **FAT32 (8–32GB)** and the UI states this explicitly.

Observable requirements:
- UI shows SD mount status, filesystem type (if detected), and free space.
- SD removal/insertion events are logged.

## The project) Backwards-Compatible Updates Promise

**Decision:** Backwards compatibility is required for v1.x in the following ways:

- **A) UI URLs and JSON fields remain stable** for all v1.x (fields may be added, not removed/renamed).
- **B) Log schema is append-only** (new optional fields may be added; existing fields are not renamed/removed).
- **C) Config schema migrates automatically** across v1.x (or the UI blocks changes with a clear error and remediation steps).

Contract:
- Firmware exposes: `firmware_version`, `config_schema_version`, `log_schema_version`, `nfc_record_version`.
- Any non-backwards-compatible change requires a v2.0+ bump and a migration note.

## J) Guided Setup Wizard (Web UI)

**Decision:** YES — provide a guided setup flow in the web UI, required until completed.

Contract:
- On first boot (or after factory reset), UI opens directly into Setup Wizard.
- Wizard verifies hardware and saves config in clear steps.
- After completion, user can re-run wizard or edit individual sections.

## K) Factory Restore (Web UI)

**Decision:** YES — provide a factory restore action.

Contract:
- Admin-only (requires Admin Config Mode).
- Requires strong confirmation (typed phrase + hold-to-confirm recommended).
- Deletes stored configuration, card allowlist, and logs per policy; restores defaults.
- Logs the factory reset event (best-effort; if logs are wiped, at minimum record a reset marker on next boot).
