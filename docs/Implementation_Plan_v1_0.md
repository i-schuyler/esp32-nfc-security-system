# Implementation Plan (V1)

This plan translates the finalized contracts in `docs/` into a build sequence designed to minimize surprises.

## Platform choice

V1 recommendation: **PlatformIO + Arduino framework**.

Reasons:
- Faster iteration on hardware bring-up and field testing
- Broad library support for PN532, SdFat (FAT32 + exFAT), DS3231, Async web server
- OTA via browser upload is straightforward
- Keeps repo approachable for future maintenance

ESP-IDF can be adopted later if a V2 requires tighter control of networking, security primitives, or performance.

## Implementation guardrails (must be followed)

The implementation must remain contract-driven. Any behavior not explicitly described in the canonical documents is not allowed to appear in code.

For each change, the implementation must confirm all of the following before writing code:
- State: the state machine explicitly allows the behavior (and defines the transition reason).
- Logs: an event exists for the behavior with required fields per the log schema.
- UI: the behavior is observable and gated correctly (read-only vs admin).
- Config: if user-togglable, the behavior has a config key with safe defaults and documented ranges.
- Bench: at least one bench test proves the behavior (including a negative-path test where applicable).

If any item above is missing, the correct action is to update documentation first and only then implement.

## Contract coverage ledger

This ledger prevents accidental omissions. Each canonical document is mapped to implementation milestones and bench checklist sections that must validate it.

| Document | Primary implementation milestones | Bench sections that validate it |
|---|---|---|
| `State_Machine_v1_0.md` | M4, M6, M7 | F, H, K |
| `NFC_Data_Contracts_v1_0.md` | M6 | E, F, K |
| `Event_Log_Schema_v1_0.md` | M2, M3, M7 | A, B, C, D, I |
| `Product_Specification_v1_0.md` | M0–M8 (all) | A–K (all) |
| `Hardware_and_Wiring_v1_0.md` | M0, M2, M4, M5, M6 | A, B, C, G, H |
| `Networking_and_Security_v1_0.md` | M1, M7 | D, F, I, K |
| `Web_UI_Spec_v1_0.md` | M1, M7 | D, F, I, J, K |
| `OTA_Update_Contract_v1_0.md` | M0, M7 | I |
| `Configuration_Registry_v1_0.md` | M1, M7 | D, G, H, K |
| `Bench_Test_Checklist_v1_0.md` | M8 | M8 executes checklist |

Supporting docs:
- `Troubleshooting_v1_0.md` is validated by negative-path tests across A–K.
- `Release_Channels_and_Stability_Promise.md` is validated during release packaging and documentation updates.
- `Platform_Choice_IDF_Note_v1_0.md` is validated by ensuring the chosen stack can satisfy the contracts without weakening guarantees.

## Milestones

Each milestone section uses a standard structure:

- Contracts to re-read: the exact documents/sections that constrain the milestone.
- Build tasks: concrete implementation tasks.
- Exit criteria: objective checks that must pass before moving on.
- Negative-path tests: failure modes that must be proven to behave predictably.
- Bench linkage: which bench checklist items are expected to pass after this milestone.


### M0 — Repo + build plumbing

**Contracts to re-read**
- `Product_Specification_v1_0.md` (scope and non-goals)
- `OTA_Update_Contract_v1_0.md` (partitioning expectations)
- `Web_UI_Spec_v1_0.md` §7 (embedded SPA served from flash)

**Build tasks**
- Create PlatformIO project that builds and flashes a minimal “version + health” firmware.
- Configure partitions for OTA and LittleFS (or equivalent flash FS) to host the embedded SPA.
- Add a build step to package UI assets into flash FS.
- CI builds firmware binary artifacts with version metadata.

**Exit criteria**
- Firmware compiles and flashes reliably.
- Firmware reports: `firmware_version`, schema versions, and boot reason in logs and UI.
- OTA partitions and flash FS are present and size-appropriate for the embedded UI.

**Negative-path tests**
- Build without UI assets present must fail loudly (not silently ship a broken UI).
- Flash FS mount failure must be visible in UI and logs.

**Bench linkage**
- A1–A3, I26 (basic log download if implemented early)

---

### M1 — Configuration + Setup Wizard

**Contracts to re-read**
- `Web_UI_Spec_v1_0.md` (Setup Wizard + Factory Restore gating, mobile-first requirement)
- `Configuration_Registry_v1_0.md` (all keys, ranges, defaults)
- `Networking_and_Security_v1_0.md` (admin gating rules)

**Build tasks**
- Implement `ConfigStore` with schema versioning and migrations for v1.x.
- Implement Setup Wizard state machine (wizard steps, completion gate, restartability).
- Persist: Wi‑Fi policy, SSID format (`Workshop Security System - A1B2`), control interface enable/disable (NFC and web), sensor enablement, output patterns, power thresholds, storage policies (exFAT support flag if applicable), and security settings.
- Implement Admin Config Mode gate and required confirmations for sensitive actions.
- Implement Factory Restore flow (admin-only, strong confirmation, post-restore wizard requirement).

**Exit criteria**
- Fresh device boots into Setup Wizard and cannot access full admin config until completed (diagnostics still visible).
- Setup Wizard can be restarted intentionally and logs session start/end.
- Factory Restore clears config and allowlists (as specified) and forces wizard again.

**Negative-path tests**
- Corrupt config storage must trigger predictable recovery (factory defaults + wizard required) and log the event.
- Attempts to change secrets/config without Admin Config Mode must be rejected and logged.

**Bench linkage**
- K29–K32 (wizard and restore), D11 (Wi‑Fi creds change), F16b (web arm)

---

### M2 — Time + storage

**Contracts to re-read**
- `Event_Log_Schema_v1_0.md` (storage tiers, rotation, retention)
- `Hardware_and_Wiring_v1_0.md` (SD/RTC expectations)
- `Troubleshooting_v1_0.md` (fault indicators)

**Build tasks**
- Integrate DS3231 RTC; implement “RTC missing/time invalid” policy and UI indicators.
- Integrate SD via SdFat:
  - Prefer FAT32 + exFAT (if feasible); otherwise clearly constrain to FAT32 8–32GB.
  - Detect filesystem type and capacity; log mount status, errors, and free space.
  - Implement fallback in-flash ring buffer when SD is missing/unavailable.
- Implement log directory/file naming and rotation scaffolding (even before full events exist).

**Exit criteria**
- RTC status is shown and logged on boot.
- SD mount/unmount events are logged; fallback logging works when SD is absent.
- If exFAT is supported, both FAT32 and exFAT cards mount and write successfully.

**Negative-path tests**
- SD removed during operation: logging continues via fallback and UI shows “SD missing”.
- RTC disconnected: time validity is reported; timestamps behavior is documented and visible.

**Bench linkage**
- B1–B2, C6–C8

---

### M3 — Event logging (tamper-aware)

**Contracts to re-read**
- `Event_Log_Schema_v1_0.md` (required fields, minimum event types, hash chaining decision)
- `Networking_and_Security_v1_0.md` (log secrecy rules for credentials)

**Build tasks**
- Implement append-only JSONL events with the required field set.
- Implement daily rotation and optional retention behavior.
- Implement hash chaining:
  - Enabled by default
  - Config option to disable
  - Hash fields included only when enabled (or explicitly set null when disabled).

**Exit criteria**
- All minimum required event types can be produced and are schema-correct.
- Hash chaining is verifiable when enabled and absent/disabled when configured off.

**Negative-path tests**
- Log write failures must be visible and must not silently drop events.
- Secrets must never appear in logs (SSID is acceptable; passwords are not).

**Bench linkage**
- A1–A3, C6–C8, D9–D10, I26

---

### M4 — State machine + outputs

**Contracts to re-read**
- `State_Machine_v1_0.md` (dominance rules, lockout orthogonal state, maintenance mode)
- `Product_Specification_v1_0.md` (latched TRIGGERED requirement)
- `Configuration_Registry_v1_0.md` (patterns and defaults)

**Build tasks**
- Implement explicit state machine with deterministic transitions and rejection logging.
- Implement TRIGGERED patterns:
  - Horn steady
  - Light steady by default
- Implement SILENCED defaults:
  - Horn off, light on, 3-minute default duration
  - All behaviors configurable via config
- Ensure outputs are OFF by default on boot until the state is known.

**Exit criteria**
- All defined transitions are implemented and logged.
- Outputs match state and pattern configuration.

**Negative-path tests**
- Invalid transitions are rejected deterministically and logged.
- State persistence across reboots is predictable and documented.

**Bench linkage**
- F16–F20, H24–H25

---

### M5 — Sensors

**Contracts to re-read**
- `Product_Specification_v1_0.md` (minimum required sensor rule)
- `Configuration_Registry_v1_0.md` (sensor enable/disable, trigger mapping)
- `Troubleshooting_v1_0.md` (sensor disconnect visibility)

**Build tasks**
- Implement sensor abstraction supporting minimum counts:
  - 2 motion sensors
  - 2 door/window sensors
  - 1 enclosure open sensor
- Enforce setup rule: at least one of (motion, door/window) must be configured for “armed correctness”.
- Implement per-sensor enable/disable and trigger mapping into event logs and state transitions.

**Exit criteria**
- UI and logs correctly identify each sensor by type and ID.
- Triggers route to TRIGGERED as specified when ARMED.

**Negative-path tests**
- Sensor disconnected: visible in UI, logged, and policy-consistent behavior (fault vs warning) is enforced.

**Bench linkage**
- G21a–G23, F17

---

### M6 — NFC (optional subsystem, must degrade gracefully)

**Contracts to re-read**
- `NFC_Data_Contracts_v1_0.md` (allowlist model, provisioning flows, capacity tiers, URL record rules)
- `Networking_and_Security_v1_0.md` (admin gating)
- `State_Machine_v1_0.md` (hold-to-confirm semantics)

**Build tasks**
- Bring up PN532 and tag scanning.
- Implement allowlist auth model with rate limiting and lockout.
- Implement hold-to-confirm gesture (3 seconds default) for sensitive actions.
- Implement provisioning flows (admin-only): add admin/user, remove card, list cards, and exit/timeout behavior.
- Implement NDEF writeback:
  - auto-detect capacity on provisioning
  - write Full → Minimal → Ultra-minimal deterministic payloads
  - preserve URL record when possible; otherwise log omission
  - overwrite/clear records each time (no accumulation)

**Exit criteria**
- NFC actions are deterministic and logged.
- Provisioning works end-to-end and logs every provisioning step.
- Truncation behavior matches the documented byte budgets.

**Negative-path tests**
- PN532 absent/disabled: system remains fully operable via web UI controls if enabled.
- Invalid scan spam triggers lockout without destabilizing other subsystems.

**Bench linkage**
- E12–E15, F16, F18–F19, K33

---

### M7 — Web UI (embedded SPA, mobile-first) + control surfaces

**Contracts to re-read**
- `Web_UI_Spec_v1_0.md` (structure, gating, SPA from flash, mobile-first)
- `Networking_and_Security_v1_0.md` (AP/STA behavior, admin mode)
- `State_Machine_v1_0.md` (web controls parity)

**Build tasks**
- Serve embedded SPA from flash; no external network dependencies.
- Implement API endpoints for:
  - status
  - events
  - config
  - admin mode / maintenance mode
  - arm/disarm/silence via web UI (must respect role/gating policy)
  - OTA upload
- Ensure UI truthfulness:
  - clearly indicates AP vs STA mode
  - shows IP and connectivity
- Enforce “control interfaces are optional”:
  - NFC can be disabled entirely during setup
  - web UI controls can be disabled (if desired) while NFC remains enabled

**Exit criteria**
- UI is usable on a phone browser with large tap targets and no horizontal scrolling.
- UI loads while phone is offline (airplane mode) as long as connected to device AP.
- Web arm/disarm/silence works and is logged identically to NFC actions.

**Negative-path tests**
- Attempt admin actions without admin mode: rejected and logged.
- Wrong STA credentials: device falls back to AP and remains reachable.

**Bench linkage**
- D9–D11, F16b, I26–I28, K29–K33

---

### M8 — Bench tests and release readiness

**Contracts to re-read**
- `Bench_Test_Checklist_v1_0.md` (all sections)
- `Release_Channels_and_Stability_Promise.md` (stable release expectations)

**Build tasks**
- Execute the full bench checklist and record results (date, hardware, notes).
- Fix failures until all mandatory items pass.
- Package release artifacts: firmware bin, checksums, version notes, and schema versions.

**Exit criteria**
- Bench checklist mandatory items pass (and negative-path tests are documented).
- Release artifact contains versioning and compatibility notes per stability promise.
