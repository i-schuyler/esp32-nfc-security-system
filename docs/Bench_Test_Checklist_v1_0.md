ARTIFACT — bench_test.md (Bench Test Checklist v1.0)

# Bench Test Checklist (V1)

Goal: prove all V1 contracts work before installation. Every item should be observable via UI and logs.

## A) Power, boot, and low-power behavior

A1. Power on device.
- Expected: boots to a known state (default DISARMED unless otherwise specified).
- Logs include: `boot` with reset reason and firmware version.
- UI shows firmware and schema versions.

A2. Power cycle (3 times).
- Expected: config remains valid; sequence behavior matches documentation.

A3. Brownout simulation (brief voltage dip).
- Expected: reset reason logged; outputs default OFF until state is known.

A4. Battery voltage measurement enabled.
- Enable in Setup Wizard.
- Expected: UI shows live battery voltage; logs include power samples.

A5. Battery voltage measurement disabled (opt-out).
- Disable in Setup Wizard.
- Expected: UI indicates measurement is disabled and no battery thresholds are enforced.

A6. Wi‑Fi disable threshold behavior (if enabled).
- Set `battery_wifi_disable_v` above current voltage to force behavior.
- Expected: Wi‑Fi transitions logged; UI indicates low-power behavior.

A7. Sleep/low-power behavior (if enabled).
- Configure sleep threshold(s) and trigger.
- Expected: sleep entry/exit is logged; system remains predictable on wake.

## B) RTC (DS3231)

B1. RTC present.
- Expected: `time_status` shows RTC OK; timestamps are valid.

B2. RTC missing or disconnected.
- Expected: FAULT or WARN per policy; UI indicates time invalid; logs show fault.

B3. `/api/status` time object.
- Expected: `time.status` is present; `time.now_iso8601_utc` is present and updates when time is valid.

## C) SD card and logging tiers

C1. SD present (FAT32, 8–32GB).
- Expected: `sd_status` mounted; log file created; events append.

C2. SD removed while running.
- Expected: log event; UI warning; fallback in-flash buffer activates.

C3. SD reinserted.
- Expected: remount event; logging resumes to SD.

C4. exFAT support (only if enabled in firmware build).
- Insert an exFAT card (>32GB).
- Expected: mounts and writes; filesystem type and capacity are logged.

C5. `/api/status` storage object.
- Expected: `storage.status` is present; `storage.sd_mounted` and `storage.active_backend` are truthful.

C6. Flash ring fallback persistence.
- Remove SD, generate a few events, reboot.
- Expected: device remains operational; storage indicates flash backend; recent events are still observable (RAM + persistent ring behavior depends on later milestones).

## D) Wi‑Fi modes and truthfulness

D1. STA join success.
- Provide known SSID/password.
- Expected: connects within timeout; UI shows STA mode and IP; `wifi_mode_change` logged.

D2. STA join fail → AP fallback.
- Provide wrong password or disable router.
- Expected: falls back to AP; UI indicates AP mode; `wifi_mode_change` logged.

D3. AP SSID format.
- Expected: SSID matches `Workshop Security System - A1B2` format (MAC suffix) unless user changed it.

D4. Change AP SSID/password (admin gated).
- Enter Admin Config Mode.
- Change SSID/password; apply.
- Expected: AP restarts with new creds; `config_change` logged without secrets.

## E) Web UI delivery model (embedded SPA) and mobile-first

E1. Embedded SPA served from flash (offline proof).
- Connect phone to device AP.
- Enable airplane mode (no internet).
- Expected: UI still loads fully and functions (status/events/config).

E2. Mobile-first usability.
- On a phone browser:
  - no horizontal scrolling
  - large tap targets
  - single-column default layout
  - status and state are readable without zoom
- Expected: layout meets the mobile-first requirement.

## F) NFC reader, roles, and provisioning (when enabled)

F1. NFC disabled/absent path (degrade gracefully).
- Disable NFC in Setup Wizard or disconnect PN532.
- Expected: system remains operable via web UI controls (if enabled); UI shows NFC disabled/absent; logs reflect status.

F2. Valid Admin card scan.
- Expected: scan logged with role; action deterministic.

F3. Valid User card scan (if enabled).
- Expected: scan logged; permissions enforced.

F4. Invalid scan spam.
- Expected: rate-limit; lockout entered; lockout logged; UI shows lockout timer.

F5. Hold-to-confirm gesture (if enabled).
- Expected: holding tag triggers the distinct action; brief tap does not.

F6. Provisioning session (admin-only).
- Add a User card, add an Admin card, remove a card.
- Expected: each step logged; allowlist updates visible in UI.

F7. Arm writeback.
- Arm via authorized NFC.
- Expected: tag receives “armed” system record; if capacity is small, minimal/ultra is used and truncation is logged.

F8. Disarm does not write back.
- Disarm via authorized NFC.
- Expected: state changes + logs; tag NDEF system record is unchanged by disarm.

F9. Incident writeback + capacity fallback.
- Trigger alarm, then clear via Admin NFC.
- Expected: incident summary written; minimal/ultra fallback occurs on smaller tags; omission of optional URL record is logged if needed.

F10. Time-invalid sentinel.
- Boot without valid RTC/time, then arm and clear.
- Expected: timestamps are `"u"` (unknown) or omitted in ultra-minimal; logs/UI reflect time invalid.

F11. PN532 presence/health.
- Boot with PN532 connected.
- Expected: NFC health is OK; reader present is true; logs show reader status.

F12. PN532 disconnect path.
- Disconnect PN532 while running (or boot without PN532).
- Expected: NFC health shows unavailable; logs show reader unavailable; system remains operable via web UI.

F13. Lockout + admin early-clear.
- Perform invalid scans until lockout triggers.
- Expected: lockout entered and logged; NFC actions ignored during lockout.
- Scan a valid Admin tag.
- Expected: lockout clears early (logged) and scan is processed normally.

F14. Hold-to-confirm clear (admin).
- Enter TRIGGERED, then hold Admin tag for the required duration.
- Expected: clear occurs only after hold threshold; short tap does not clear; logs show hold action.

F15. Provisioning session add/remove.
- Enter Admin Config Mode.
- Start provisioning session; add Admin and User tags; remove a tag.
- Expected: provisioning events logged with tag prefix only; allowlist persists across reboot.

F16. Clear rejected on writeback fail.
- Force writeback failure (e.g., remove tag mid-write or use an unwritable tag).
- Attempt Admin hold clear.
- Expected: clear rejected; writeback failure logged; state remains TRIGGERED.

F17. Tag capacity fallback coverage.
- Use at least one small Type 2 tag (e.g., NTAG213) and one larger (e.g., NTAG215/216).
- Expected: larger tag uses full payload; smaller tag uses minimal/ultra with truncation logged.

F18. URL record preservation behavior.
- Enable URL record; perform clear with adequate capacity.
- Expected: URL record preserved alongside system record; omission is logged if capacity is too small.
- Disable URL record; perform clear.
- Expected: only system record is written.

## G) State machine and control parity (NFC and web)

G1. DISARMED → ARMED via NFC (if enabled).
- Expected: transition logged; UI updates; “armed record” written to NFC.

G2. DISARMED → ARMED via web UI (if enabled).
- Enter Admin Config Mode.
- Click Arm.
- Expected: transition logged; UI updates; behavior matches NFC path.

G3. ARMED → TRIGGERED via sensor trigger.
- Expected: outputs active per patterns; incident event logged.

G4. TRIGGERED → SILENCED via NFC or web (if enabled).
- Expected: horn off (default), light remains on (default); duration defaults to 3 minutes unless changed; log event present.

G5. TRIGGERED → DISARMED via NFC “Clear” (admin).
- Expected: outputs off; incident summary logged; NFC incident writeback updated.

G6. FAULT dominance.
- Induce fault (e.g., disconnect RTC or SD per policy).
- Expected: FAULT shown; logged; behavior matches policy.

G7. Web control endpoints are no longer stubs (M4).
- Enter Admin Config Mode.
- Call: `POST /api/control/arm`, `.../disarm`, `.../silence`.
- Expected: HTTP 200 on valid transitions; HTTP 409 on invalid transitions.
- Logs include: `state_transition` events with `from/to/reason`.

G8. Silence duration expiry (M4).
- Force state to SILENCED (e.g., trigger → silence).
- Expected: after `silenced_duration_s` (default 180s), state returns to the pre-silence state.
- Logs include: `state_transition` with reason `silence_expired`.

G9. State persistence across reboot (M4).
- Set state ARMED, reboot.
- Expected: device comes back ARMED (no silent disarm); outputs remain OFF (ARMED).
- Set state TRIGGERED (later milestone trigger), reboot.
- Expected: device comes back TRIGGERED.

G10. `/api/status` state machine fields (M4).
- Expected: `last_transition` object is present (append-only).
- Expected: `silenced_remaining_s` is present (0 when not silenced).
- Expected: `fault` object is present.

## H) Sensors

H1. Minimum supported sensor count.
- Configure: 2 motion sensors, 2 door/window sensors, and 1 enclosure open sensor.
- Expected: `/api/status.sensors.sensors[]` lists each sensor with stable `type` and `id` values, and shows `enabled_cfg`, `pin_configured`, and `health`.
- Expected: triggers log correct source IDs (`sensor_type`, `sensor_id`).

H2. Minimum required sensor rule.
- Attempt to complete Setup Wizard with no primary sensor enabled.
- Expected: wizard blocks completion (or forces explicit override if allowed) and logs reason.

H2b. Arm blocked when no primary sensor enabled (M5).
- Disable all primary sensors (`motion_enabled=false`, `door_enabled=false`, and per-sensor enables false).
- Attempt to arm via Web UI control endpoint.
- Expected: arm is rejected (HTTP 409) and a `state` warning log `arm_blocked` is present.

H2c. Unconfigured sensor visibility (M5).
- Enable a sensor in config but leave its pin unset (`-1` default pin map).
- Expected: `/api/status.sensors.overall` becomes `unconfigured` and at least one warning log is present (`sensor_unconfigured` or `sensor_pin_unset`).

H3. Motion sensor sensitivity calibration.
- Expected: can adjust in Admin mode; change logged; live readings visible.

H4. Door/window sensor trigger (if installed).
- Open/close; expected: state reflects; triggers per policy.

H5. Enclosure/tamper trigger (if installed).
- Open enclosure; expected: tamper event logged; triggers per policy.

H6. Sensor trigger logging fields (M5).
- With a sensor pin configured and changing state, induce a rising-edge trigger.
- Expected: a log event with type `sensor_trigger` includes `sensor_type` and `sensor_id` (and raw/active fields).

H7. Trigger routes into state machine when ARMED (M5).
- Set state to ARMED.
- Induce a sensor trigger.
- Expected: state transitions to TRIGGERED and logs show `sensor_trigger` followed by `state_transition`.

## I) Outputs and test mode

I1. Test horn/light outputs in maintenance/test mode.
- Expected: buttons work; outputs match; logs `test_action`.

I2. Output default-off on boot.
- Power cycle while horn/light connected.
- Expected: no accidental chirps beyond documented boot behavior.

I3. `/api/status` outputs object (M4).
- Expected: `outputs.horn_pin_configured` and `outputs.light_pin_configured` reflect pin config.
- Expected: `outputs.horn_active` / `outputs.light_active` reflect effective state.

I3. Output pin-map disabled behavior (M4).
- Leave `WSS_PIN_HORN_OUT`/`WSS_PIN_LIGHT_OUT` unset (-1).
- Expected: `outputs.*_pin_configured=false` in `/api/status` and no GPIO activity.

I4. Output policy linkage to state machine (M4).
- With pins configured, trigger TRIGGERED and SILENCED states.
- Expected: TRIGGERED uses `horn_pattern`/`light_pattern` (default steady);
  SILENCED forces horn OFF and uses `silenced_light_pattern` for light.

## J) Logs, downloads, and tamper-aware properties

J1. Download logs.
- Expected: downloads JSONL (or documented format) via UI.

J1b. Daily file header event (when hash chaining enabled).
- Expected: first JSONL line per day is `file_header` and includes schema/version metadata; `prev_hash` is fixed zeros for the file start.

J2. Hash chaining enabled (default).
- Trigger at least 10 log events.
- Expected: each event includes `hash` and `prev_hash` and the chain is continuous.

J3. Hash chaining disabled.
- Disable in config and reboot.
- Expected: hash fields are absent (or explicitly null) and logging remains correct.

## K) OTA update

K1. OTA update happy path.
- Upload valid `.bin`.
- Expected: `ota_update` start/finish logged; UI shows result; device remains reachable.

K2. OTA update failure path.
- Upload corrupt/wrong `.bin`.
- Expected: fails safely; device remains reachable; failure logged.

## L) Setup Wizard, Factory Restore, and NFC URL record

L1. Setup Wizard required until completion.
- Fresh boot or after factory restore.
- Expected: UI opens to Setup Wizard; full admin config blocked until completion (diagnostics allowed).

L2. Setup Wizard completion.
- Complete all steps.
- Expected: `setup_completed=true`; summary stored; `setup_step` and `config_change` logged.

L3. Re-run Setup Wizard.
- Expected: admin-only; wizard resets `setup_last_step`; logs session start/end.

L4. Factory restore.
- In Admin Config Mode, perform factory restore with required confirmation.
- Expected: config reset; allowlist cleared; Setup Wizard required again; reset logged (or reset marker on next boot).

L5. Optional NFC URL record (AP default).
- Enable `nfc_url_record_enabled` and set URL mode to AP default.
- Expected: phone tap opens `http://192.168.4.1/` when connected to AP; device preserves URL record when rewriting system records, or logs omission due to capacity.

L6. Admin Config Mode entry without NFC.
- Disable NFC control interface (or run without NFC hardware).
- Complete Setup Wizard and set the web admin password.
- Expected: web UI can enter Admin Config Mode via password; session expires per `admin_mode_timeout_s`; admin-only endpoints reject requests without token.

L7. No-secrets enforcement (UI + logs).
- Set STA credentials and AP password via Setup Wizard.
- Expected: event logs contain `config_change` keys only (no password values); `/api/config` redacts secret fields; Wi‑Fi SSID may be visible.

L8. Corrupt config recovery (negative path).
- Corrupt/erase config storage (NVS) and reboot.
- Expected: device restores defaults, requires Setup Wizard, and logs a `config_corrupt_or_reset` (or equivalent) event.

## M7 Addendum (M7)

M7.1 Admin eligibility + password (two-step, offline AP).
- Connect to device AP; open the UI.
- Scan a valid Admin NFC tag.
- Expected: Status shows `Admin: Eligible (<remaining>)`.
- Enter Admin password.
- Expected: Status shows `Admin: Authenticated (<remaining>)`; admin-only sections become visible.
- Failure signal: password alone grants admin without an eligible window, or eligible window appears without NFC.

## M7.1 Addendum (M7.1) — /setup Route Gating

Preconditions:
- Device booted; UI available over AP or STA; no external internet required.
- Ability to set `setup_required` true/false via normal setup completion state (no new flags).

A) When `setup_required == true`:
- Actions:
  - Visit `/`.
  - Visit `/index.html` or any other UI route (e.g., `/foo`).
  - Visit `/setup` directly.
  - Observe the Setup Wizard steps with missing hardware (e.g., RTC or SD absent).
  - Call `/api/status` directly.
- Expected:
  - `/` and other UI routes redirect to `/setup`.
  - `/setup` does not redirect away (loop protection).
  - `/setup` shows a calm "why you're here" message explaining setup is required.
  - Wizard steps display "Unknown" where hardware is missing, with guided instructions; no step is blocked.
  - `/api/status` returns JSON (no redirect to `/setup`).
- Failure signals:
  - Any UI route bypasses `/setup` while setup is required.
  - `/setup` redirects away or loops.
  - Missing hardware blocks progress instead of showing "Unknown" with guidance.
  - `/api/status` redirects to `/setup` or returns HTML.

B) When `setup_required == false`:
- Actions:
  - Visit `/`.
  - Visit `/setup`.
  - Inspect `/setup` for sensitive fields (passwords/tokens/UID).
  - Check re-run button state while Admin Eligible vs Admin Authenticated.
- Expected:
  - `/` loads the normal landing page (no redirect to `/setup`).
  - `/setup` shows read-only "Setup completed" view.
  - Sensitive fields are hidden (no Wi-Fi passwords, admin tokens, or raw NFC UID).
  - "Re-run setup" is disabled/blocked unless Admin Authenticated is active.
- Failure signals:
  - `/` redirects to `/setup` after setup is complete.
  - `/setup` shows editable setup fields without Admin Authenticated.
  - Any secret or UID is visible (use [REDACTED] in test notes).

C) Escape hatch:
- Actions:
  - From `/setup`, open the Diagnostics section.
- Expected:
  - Diagnostics is read-only and does not expose privileged actions.
- Failure signals:
  - Diagnostics allows admin actions or reveals secrets.

## M7.2 Addendum (M7.2) — Setup Wizard Optimization (offline AP)

Preconditions:
- Device in AP mode; phone connected; no external internet.
- `setup_required == true` for A–E; `setup_required == false` for F (after completion).

A) No snapback / no step mutation due to refresh.
- Actions:
  - Open `/setup`, navigate to a mid wizard step, and click "Save step".
  - Wait for any periodic refresh window (at least 60s), then refresh the browser.
- Expected:
  - Wizard remains on the current step; navigation selection does not change.
  - Only read-only status indicators update (if present); no jump to last-saved step.
- Failure signals:
  - Step changes or navigation snaps back without operator action.

B) Step order (critical first).
- Actions:
  - Inspect the step list/order.
- Expected:
  - Step 1: Welcome + Admin Password.
  - Step 2: Network (AP password change required; SSID change optional; STA optional).
  - Step 3: Inputs (NFC + Sensors).
  - Step 4: Time & RTC.
  - Step 5: Storage.
  - Step 6: Outputs.
  - Step 7: Review & Complete.
  - Inputs step covers NFC provisioning and sensor enablement (no separate NFC step).
- Failure signal:
  - Order differs or a critical step is missing.

C) Navigation not blocked by later steps.
- Actions:
  - Leave optional fields empty and proceed forward.
  - Simulate missing hardware (e.g., RTC or SD absent) and proceed.
- Expected:
  - Operator can continue through steps without being blocked by later steps.
  - Missing hardware shows "Unknown" with guided instructions.
- Failure signals:
  - Progress blocked by missing hardware or unvisited later steps.

D) Complete button gating.
- Actions:
  - Attempt completion from a non-final step.
  - Go to the final step without visiting every prior step.
- Expected:
  - "Complete setup" appears only on the last step and only after all steps have been visited.
- Failure signals:
  - "Complete setup" appears early or before all steps are visited.

E) Completion requirements enforcement (LOCKED).
- Actions:
  - Attempt to complete with default admin password, default AP password, or no primary sensor enabled.
- Expected:
  - Completion is blocked with guided reasons; relevant `setup_step` or `config_change` logs are present (no secrets).
- Failure signals:
  - Completion succeeds without meeting all requirements.

F) `/setup` routing behaviors and loop protection.
- Actions:
  - With `setup_required == true`, visit `/`, other UI routes, and `/setup`.
  - With `setup_required == false`, visit `/` and `/setup`; check the re-run button state.
- Expected:
  - When setup is required: `/` and other UI routes redirect to `/setup`; `/setup` never redirects away.
  - When setup is complete: `/` is default; `/setup` is read-only "Setup completed"; re-run requires Admin Authenticated.
- Failure signals:
  - Any route bypasses `/setup` when required, loop protection fails, or re-run is allowed without Admin Authenticated.

G) No secrets / no UID displayed or logged.
- Actions:
  - Enter Wi-Fi credentials in the wizard; observe UI fields and recent event logs.
  - Download logs if available and scan for secrets or raw UID.
- Expected:
  - No Wi-Fi passwords, tokens, or raw UID shown in UI or logs.
  - Use [REDACTED] in test notes if any value is observed.
- Failure signal:
  - Any secret or UID appears in UI, `/api/events`, or downloaded logs.

M7.2 Admin eligibility timeout behavior.
- Scan a valid Admin NFC tag.
- Wait for eligibility to expire without logging in.
- Expected: Status returns to `Admin: Off`; admin-only actions remain blocked.
- Failure signal: admin actions remain allowed after eligibility expires.

M7.3 Log export (SD present, offline AP).
- Ensure SD is mounted and active.
- In Admin mode, use Download logs: Today, Last 7 days, All (may be limited).
- Expected: each download returns a `.txt` file with JSONL lines; no buffering delays.
- Expected: if size exceeds limit, response is HTTP 409 with: “Too large to download. Choose a shorter range.”
- Failure signal: download succeeds without Admin mode, or returns secrets.

M7.4 Log export (SD missing, flash fallback).
- Remove SD and reboot (or ensure fallback is active).
- In Admin mode, download Today.
- Expected: file starts with header line `# FLASH_FALLBACK_LOG_SNAPSHOT (most recent entries)`.
- Failure signal: download fails silently or includes raw secrets.

M7.5 OTA upload (admin-only, streamed, reboot-on-success).
- In Admin mode, upload a valid `.bin`.
- Expected: UI shows result “Rebooting now. Reconnect to the device Wi‑Fi.”; device reboots.
- Expected: OTA events logged with start/result, no firmware contents logged.
- Failure signal: OTA allowed without Admin mode, or no reboot on success.

M7.6 Output tests (admin-only, 5s timeout + revert).
- In Admin mode, press “Test horn (5s)” then “Test light (5s)”.
- Expected: outputs activate for ~5s and stop automatically; no state transition occurs.
- Press “Stop tests” during an active test.
- Expected: outputs stop immediately; normal output policy resumes.
- Failure signal: outputs remain on, or alarm state changes.

M7.7 UI section order + “Unknown” handling.
- Verify section order: Status → Recent Events → Controls → Config → Diagnostics → Maintenance.
- Force time invalid (RTC missing) and reload UI.
- Expected: Status shows “Unknown” (never shows `"u"`), and time helper text appears.
- Failure signal: “u” appears or sections are out of order.

M7.8 No-secrets and no-UID exposure (UI + logs).
- Trigger admin/login, log export, and OTA/upload attempts.
- Expected: no raw NFC UID, passwords, or tokens appear in UI, `/api/events`, or downloaded logs.
- Failure signal: any raw UID or secret value is visible.

## M) End-to-end regression set (recommended)

After any firmware change, rerun:
- A1–A3, C1–C3, D1–D2, F2–F5 (if NFC enabled), G1–G6, J1–J3, K1–K2, L1–L4
- A1–A3, C1–C3, D1–D2, F2–F5 (if NFC enabled), G1–G6, J1–J3, K1–K2, L1–L8
