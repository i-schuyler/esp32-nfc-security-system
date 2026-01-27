ARTIFACT — ui_spec.md (Web UI Spec v1.0)

# Web UI Specification (V1)

**Important:** The project requires that **no code is generated** until you approve the UI text and visual style.
This doc therefore defines **structure + behavior contracts**, and marks any unapproved copy/styling as **PENDING APPROVAL**.

## 1) UI Goals

- Legible, calm, and explicit.
- No hidden state transitions.
- Default view is read-only status.
- Admin actions are clearly gated and labeled.
- Mobile-first: designed for phone screens first (large tap targets, single-column layout, minimal scrolling for status).

## 2) Navigation Layout (Canonical Sections)

1) **Status**
   - Current state (DISARMED/ARMED/TRIGGERED/SILENCED/FAULT)
   - Last transition reason + timestamp
   - Active faults (if any)
   - Network mode (AP/STA) + IP
   - SD/RTC status indicators

2) **Recent Events**
   - Last N log events (filterable)
   - Download logs (today / last 7 days / all)

3) **Controls**
   - Visible controls depend on Admin Config Mode:
     - Read-only: none or safe diagnostics
     - Admin: output tests, maintenance/test mode controls

Control interfaces (V1):
- If web control is enabled, Admin mode exposes Arm / Disarm / Silence actions.
- If NFC control is enabled, NFC actions remain available as defined in the state machine.
- Setup Wizard allows disabling NFC control (for installs without an NFC reader/tags) and/or disabling web control.


4) **Config (Admin only)**
   - Wi‑Fi SSID/password management
   - Sensor sensitivity/calibration
   - Output patterns
   - Lockout parameters

5) **Diagnostics**
   - Firmware version + schema versions
   - Reset reason
   - SD capacity
   - RTC time
   - Live sensor readings (safe)

## 3) Tagline / Welcome Message (APPROVED)
The UI includes a single short block of text at the top of the UI.

Selected V1 default:
“Calm system. Clear state. Durable logs.”

Notes:
- This text is treated as UI copy and can be customized by the administrator.

## 4) Admin Config Mode Banner (Canonical)

When Admin Config Mode is active:
- display a clear banner
- show countdown until expiry
- provide a button to exit admin mode early

## 5) Endpoint Contracts (High-level)

UI should consume a small set of endpoints:

- `GET /api/status`
- `GET /api/events?limit=...`
- `GET /api/config` (admin only)
- `POST /api/config` (admin only)
- `POST /api/test/*` (admin only)
- `POST /api/ota` (admin only, likely)
- `GET /download/logs?...`

**Note:** exact URL names can be changed, but once v1.0 ships, they are part of the backwards-compat contract for v1.x.

## 6) UI Safety Rules

- Any “dangerous” action requires a confirmation step (two-click).
- OTA upload shows filename, size, and result.
- Config pages show:
  - description, units, ranges, safe defaults, and risk notes
- Test mode is clearly labeled as non-security behavior.

## 7) Offline + Caching
The UI must work in AP mode with no internet.

V1 requirement:
- The UI is embedded as a single-page app served from flash (no external assets required).
- The UI must load reliably on mobile browsers without network connectivity beyond the device link.

## 8) Guided Setup Wizard (Required until completed)

**Goal:** a calm, step-by-step setup initiated in the web UI that prevents half-configured installs.

### Wizard entry rules
- On first boot (or after factory restore), UI loads directly into **Setup Wizard**.
- Wizard completion status is stored as `setup_completed=true`.
- User can re-run wizard at any time (Admin only), or edit specific sections without re-running the entire wizard.

### Wizard steps (proposed V1)
1) **Welcome + Safety** (explains no hidden behavior; shows current state)
2) **Time & RTC** (RTC detected? set timezone; verify timestamp)
3) **Storage** (SD detected? logging status; choose retention)
4) **Network** (set STA credentials (optional), set AP SSID/password; apply safely)
5) **NFC** (add Admin card(s), add User card(s) if desired; confirm permissions)
6) **Sensors** (enable motion/door/tamper; basic calibration/sensitivity)
7) **Outputs** (test horn/light; set default patterns)
8) **Review** (shows summary; user confirms; marks setup complete)

All wizard actions must be logged as `config_change` or `setup_step`.

## 9) Factory Restore (Admin only)

A factory restore resets the device to a clean state.

Contract:
- Requires Admin Config Mode active.
- Requires strong confirmation (typed phrase + hold-to-confirm recommended).
- Resets: configuration, card allowlist, and (optionally) logs.
- After restore, Setup Wizard is required again.
