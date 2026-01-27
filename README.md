# ESP32 NFC Security System (V1)

This repository contains firmware and documentation for an ESP32-based workshop security system controlled via NFC and a local-only, mobile-first web UI.

## Scope (V1)
- Offline-first operation (AP + optional STA)
- Explicit state machine (DISARMED / ARMED / TRIGGERED / SILENCED / FAULT)
- NFC + Web UI control interfaces (each can be disabled during setup)
- Tamper-aware, SD-backed event logging with optional hash chaining
- Guided Setup Wizard (required until completed)
- OTA firmware update via the local web UI

## Repo layout
- `docs/` — product specification + contracts (source of truth)
- `firmware/` — PlatformIO (Arduino framework) firmware
- `ui/` — single-page app source (built and embedded into firmware)
- `tools/` — helper scripts (build, embed UI, release packaging)

## Build (developer quickstart)
See `docs/Implementation_Plan_v1_0.md` for the recommended workflow and milestones.


## Development (M0 scaffolding)

This repo currently contains M0 scaffolding:
- PlatformIO + Arduino project
- Embedded single-page web UI served from flash (LittleFS)
- Minimal `/api/status` endpoint

### Upload steps (developer)

```bash
pio run -t upload
pio run -t uploadfs
pio device monitor
```

### Bootstrapping Wi‑Fi behavior (temporary)

M0 includes a temporary AP bootstrap so the UI can be reached during early bring-up.
- SSID format: `Workshop Security System - A1B2` (MAC suffix)
- Password: `ChangeMe-A1B2` (derived from the same suffix)

This is not a product default and will be replaced by the Setup Wizard in M1.

## M1 Notes

M1 implements the Configuration + Setup Wizard milestone.

### Endpoints introduced or expanded

Status + diagnostics:
- `GET /api/status` — now includes setup/admin indicators and feature flags
- `GET /api/events` — recent events (RAM ring buffer; persistent logging arrives in later milestones)

Wizard:
- `GET /api/wizard/status` — wizard required / last step
- `POST /api/wizard/step` — persist a wizard step (no secrets logged)
- `POST /api/wizard/complete` — marks setup complete (enforces primary sensor enabled + admin password set)

Admin Config Mode:
- `GET /api/admin/status` — session status
- `POST /api/admin/login` — enter Admin Config Mode via web admin password (wizard-set)
- `POST /api/admin/logout` — exit Admin Config Mode

Config + restore:
- `GET /api/config` — redacted config (Admin Config Mode required)
- `POST /api/config` — apply patch (Admin Config Mode required; secrets never logged)
- `POST /api/factory_restore` — factory restore (Admin Config Mode required + hold-to-confirm)

Control parity scaffolding (explicit stubs until M4 state machine):
- `POST /api/control/arm`
- `POST /api/control/disarm`
- `POST /api/control/silence`

### Explicit stubs in M1
- State machine actions (arm/disarm/silence) are present in the UI/API but return `stub=true` until M4.
- NFC allowlist reset is wired into Factory Restore, but allowlist storage/provisioning is implemented in later milestones.
- Power/outputs wizard steps are UI-visible and persist keys, but hardware behavior is not activated in M1.

### Migration/version notes
- Config is stored as JSON in NVS with `schema_version=1`.
- v1.x forward migrations are scaffolded via a migration hook (no-op in M1).

## M2 Notes

M2 adds time and storage scaffolding so logs become durable and time-stamped without introducing silent behavior.

What was added
- DS3231 RTC support (best-effort) with explicit status reporting (`OK` / `TIME_INVALID` / `MISSING` / `DISABLED`).
- SD logging support (SdFat) with explicit status reporting (`OK` / `MISSING` / `ERROR` / `DISABLED`) and a persistent flash fallback ring buffer when SD is unavailable.
- Log directory/file naming + daily rotation scaffolding (events written as JSON lines into `/logs/YYYY/MM/events_YYYY-MM-DD.txt`).
- Setup wizard steps for **Time & RTC** and **Storage** (structure/behavior only; minimal copy).

Endpoints introduced/changed
- `GET /api/status` now includes `time` and `storage` objects.
- `POST /api/time/set` sets device time (and adjusts RTC if present). Requires Admin Config Mode after setup; during setup it is allowed without admin.
- `POST /api/wizard/step` accepts an action key `rtc_set_epoch_s` for the `time` step (sets time; it is not stored as config).

Explicit stubs / wiring notes
- Pin mapping is intentionally not assumed (canonical pin map is TBD). RTC/SD will report `DISABLED` until pins are provided via build flags (see `src/config/pin_config.h`).
- Hash-chained durable logs remain a later milestone (M3). M2 persists the existing JSON event lines.

Migration/version notes
- No config schema version change in M2. Existing config keys `timezone`, `sd_required`, and `log_retention_days` are used.

## M3 Notes

M3 strengthens evidence durability by making SD-backed logs tamper-evident via hash chaining, while keeping behavior offline-first and append-only.

What was added
- Hash chaining for persisted JSONL events (enabled by default; controlled via `hash_chain_logs`).
- Per-day `file_header` event (written when hash chaining is enabled and a new daily log file is created).
- Best-effort log retention enforcement on SD (`log_retention_days`) when time is valid.
- Additional storage diagnostics fields to make log write failures and hash state observable.

Endpoints introduced/changed
- `GET /api/status` storage object now also includes UI-friendly aliases: `storage.status` and `storage.free_mb` (append-only), plus M3 diagnostics:
  - `storage.hash_chain_enabled`, `storage.chain_head_hash`, `storage.write_fail_count`, `storage.last_write_ok`, `storage.last_write_backend`, `storage.last_write_error`.

Explicit stubs / wiring notes
- State machine and sensor/NFC-driven event types remain explicit stubs until later milestones; M3 focuses on log integrity and observability.

Migration/version notes
- No schema version changes in M3. The `hash` and `prev_hash` fields are added to JSONL events when hashing is enabled (or explicitly set to null when disabled).

## M4 Notes

M4 implements the explicit alarm state machine and deterministic horn/light output control.

What was added
- A first-class state machine with persisted state: `DISARMED`, `ARMED`, `TRIGGERED`, `SILENCED`, `FAULT`.
- Web control parity (admin-gated) for `arm`, `disarm`, and `silence` (no longer stubs).
- Deterministic outputs manager (horn + light) driven by state and configuration (pins default unset, so outputs are disabled unless explicitly configured).

Endpoints introduced/changed
- `GET /api/status` now includes (append-only):
  - `last_transition` object (`ts`, `time_valid`, `from`, `to`, `reason`)
  - `silenced_remaining_s`
  - `fault` object (`active`, `code`, `detail`)
  - `outputs` object (pin/config/active flags and patterns)
- `POST /api/control/arm` now performs a real transition `DISARMED -> ARMED`.
- `POST /api/control/disarm` now performs a real transition `ARMED|SILENCED -> DISARMED`.
- `POST /api/control/silence` now performs a real transition `ARMED|TRIGGERED -> SILENCED` (temporary; returns to the pre-silence state).

Explicit stubs / wiring notes
- Canonical pin mapping remains TBD. Horn and light GPIOs default to `-1` (unset) and are treated as disabled.
- Non-`steady` output patterns fall back to `steady` with an explicit warning log (`pattern_unimplemented`).
- Sensor trigger and NFC clear/writeback are implemented in later milestones; M4 provides the state-machine hooks (`trigger`, `clear`) without enabling new behavior silently.

Migration/version notes
- No schema version changes in M4. State is persisted in NVS under the `wss_state` namespace.

## M5 Notes

M5 implements the sensor abstraction layer and per-sensor enable/disable scaffolding, making sensor triggers and health observable while preserving the "pin map TBD" constraint.

What was added
- `src/sensors/sensor_manager.*` introduces a simple, deterministic sensor manager with per-sensor IDs.
- Per-sensor enable keys were added to config (legacy `motion_enabled`/`door_enabled` are still supported for backwards compatibility).
- Sensor triggers now emit structured log events (`sensor_trigger`) and route into the state machine when ARMED.
- `/api/status` now includes a `sensors` object describing sensor configuration and health.

Endpoints introduced/changed
- `GET /api/status` now includes (append-only): `sensors.overall`, `sensors.any_primary_enabled_cfg`, `sensors.any_primary_configured`, and `sensors.sensors[]`.

Explicit stubs / wiring notes
- Canonical sensor pin mapping remains TBD. Sensor pins default to `-1` (unset) and are reported as `unconfigured` when enabled.
- Digital sensor polarity/pull-mode is configurable via append-only config keys; no hardware assumptions are made beyond this generic GPIO model.

Migration/version notes
- No schema version changes in M5. New config keys are defaulted on load for older configs (append-only behavior).
