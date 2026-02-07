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

### Routing + gating (M7.1)
- Setup Wizard is served at `/setup` (separate page in the UI).
- UI gating uses existing backend status flag: `setup_required` (from `GET /api/status`).
- If `setup_required == true`:
  - Visiting `/` redirects to `/setup`.
  - Visiting any other UI route redirects to `/setup`.
  - Visiting `/setup` never redirects away from `/setup` (loop protection).
  - `/setup` shows a Mode C "why you're here" message with an explicit reason and next step.
    Example (PENDING APPROVAL): "Setup is required because this device is not fully configured yet. Complete the steps below to make it ready for use."
- If `setup_required == false`:
  - `/` is the default landing page.
  - `/setup` renders a read-only "Setup completed" view; sensitive fields are hidden.
  - Re-run setup requires **Admin Authenticated** (not merely Admin Eligible).
  - `/setup` provides a "Re-run setup" button; when not Admin Authenticated, show a locked state with helper text.

### Wizard entry rules
- On first boot (or after factory restore), `setup_required` is true and UI loads directly into **Setup Wizard**.
- Wizard completion status is stored in config (see `Configuration_Registry_v1_0.md`) and exposed as `setup_required`.
- `setup_last_step` (when present) is used to resume or highlight the next step on initial entry only.
- User can re-run wizard at any time (Admin Authenticated only), or edit specific sections without re-running the entire wizard.

### Wizard steps (V1, M7.2 order)
1) **Welcome + Admin Password** (merged; guided operator instructions)
2) **Network** (change AP password from default; SSID change optional; STA optional)
3) **Time & RTC** (RTC detected? set timezone; verify timestamp; M7.3 includes RTC pin assignment)
4) **Storage** (SD detected? logging status; choose retention; M7.3 includes SD SPI pin assignment)
5) **NFC** (add Admin card(s), add User card(s) if desired; confirm permissions; M7.3 includes PN532 SPI module selection + pin assignment, with optional IRQ/RST)
6) **Sensors** (enable motion/door/tamper; basic calibration/sensitivity; M7.3 motion selection includes LD2410B UART pin assignment)
7) **Outputs** (test horn/light; set default patterns)
8) **Review + Complete** (summary + final validation; marks setup complete)

All wizard actions must be logged as `config_change` or `setup_step`.

### M7.2 Setup Wizard Optimization Contract (M7.2)

A) Auto-refresh policy
- Auto-refresh must never change wizard navigation or step selection.
- Any refresh used must be read-only and must not snap back to the last-saved step.
- Decision: remove step-mutating auto-refresh; allow minimal status refresh only if needed.

B) Step order (critical first)
- Step 1: Welcome + Admin Password (merged) with guided operator instructions.
- Step 2: Network; change AP password from default (required); SSID change optional; STA optional.
- Step 3: Inputs (NFC + Sensors); merged step with guided instructions; includes pin assignment (NFC + sensors), PN532 SPI module selection for M7.3, and LD2410B UART pin selection; never blocked by missing hardware.
- Step 4: Time & RTC; includes RTC pin selection for M7.3.
- Step 5: Storage; includes SD SPI pin selection for M7.3.
- Step 6: Outputs.
- Step 7: Review & Complete.
- Hardware steps must never be blocked by missing hardware; show "Unknown" with guided instructions.

C) Navigation rules
- Operator can proceed through steps without being blocked by later steps.
- "Save step" persists current step inputs and remains on the same step; it is not time-sensitive.
- "Continue" advances to the next step after saving current inputs.
- "Complete setup" appears only on the last step and only after all steps have been visited at least once.

D) Completion requirements (LOCKED)
- Admin password set.
- AP password changed from default.
- At least one primary sensor enabled.
- Every wizard step visited at least once.

E) Save vs Complete semantics (Mode C)
- "Save step": persist current step inputs without finishing the wizard; use when pausing or waiting on hardware.
- "Continue": advance to the next step after saving current inputs.
- "Complete setup": final validation + mark setup completed; on success, return to `/` (home).

F) Persistence across OTA (LOCKED V1)
- Configuration is stored in NVS and persists across OTA updates.
- SD+NVS dual save deferred to V2.

G) `/setup` routing and read-only post-completion
- If `setup_required == true` (or `setup_completed == false`): `/` and other UI routes redirect to `/setup`; `/setup` never redirects away (loop protection).
- If setup is complete: `/` is default; `/setup` renders read-only "Setup completed" with sensitive fields hidden.
- Re-run setup requires Admin Authenticated; `/setup` shows a locked state when not authenticated.

H) Security boundary reminders
- `/setup` never displays secrets (Wi-Fi passwords, tokens, raw UID).
- Re-run setup requires Admin Authenticated (not merely Admin Eligible).

I) Save failure messaging (Mode C)
- If saving fails, the UI must clearly say settings were not saved and must not silently imply persistence.

## 9) Factory Restore (Admin only)

A factory restore resets the device to a clean state.

Contract:
- Requires Admin Config Mode active.
- Requires strong confirmation (typed phrase + hold-to-confirm recommended).
- Resets: configuration, card allowlist, and (optionally) logs.
- After restore, Setup Wizard is required again.
