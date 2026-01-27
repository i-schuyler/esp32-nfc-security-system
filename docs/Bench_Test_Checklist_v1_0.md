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

## M) End-to-end regression set (recommended)

After any firmware change, rerun:
- A1–A3, C1–C3, D1–D2, F2–F5 (if NFC enabled), G1–G6, J1–J3, K1–K2, L1–L4
- A1–A3, C1–C3, D1–D2, F2–F5 (if NFC enabled), G1–G6, J1–J3, K1–K2, L1–L8
