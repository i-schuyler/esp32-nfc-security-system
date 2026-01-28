# ESP32 NFC Security System (V1)

Firmware and documentation for an ESP32-based workshop security system controlled via NFC and a **local-only, mobile-first web UI**.

This repo is intentionally offline-first: it is designed to run without cloud dependencies. Any “cloud” integration, if ever added, must be explicit and opt-in.

## Status
- Milestones **M0–M5** are implemented and merged.
- **M6–M8 are paused** until planning/spec work resumes and bench testing begins.

## Key properties (V1 scope)
- Offline-first operation (AP + optional STA)
- Explicit state machine (DISARMED / ARMED / TRIGGERED / SILENCED / FAULT)
- NFC + Web UI control interfaces (each can be disabled during setup)
- Tamper-aware, SD-backed event logging with optional hash chaining
- Guided Setup Wizard (required until completed)
- OTA firmware update via the local web UI (planned/contracted in docs)

## Repo layout (source of truth)
- `docs/` — specification + contracts + checklists (canonical)
- `src/` — firmware source (PlatformIO / Arduino framework)
- `data/` — web UI files embedded via LittleFS (served by firmware)
- `tools/` — helper scripts (developer sanity checks, packaging helpers)
- `.github/workflows/` — GitHub Actions CI (compile-check + optional artifact builds)
- `AGENTS.md` — Codex guardrails / “how to work in this repo safely”

Notes:
- A placeholder `/UI/` directory may exist with a `.keep` file for future work; the **current embedded UI lives in `data/`**.

## Quickstart (developer)
Prereqs:
- PlatformIO CLI available (`pio`)

Build (compile):
```bash
pio run -e esp32dev
```

Upload firmware:
```bash
pio run -e esp32dev -t upload
```

Upload filesystem (LittleFS UI assets):
```bash
pio run -e esp32dev -t uploadfs
```

Serial monitor:
```bash
pio device monitor
```

### CI behavior
Default CI runs a compile-check (no hardware flashing) on pushes/PRs. Artifact-producing builds (e.g., firmware binaries / filesystem images) are intentionally optional so the pipeline stays fast and low-friction.

### Codex + ChatGPT workflow (guardrails)
* **ChatGPT**: planning, specs/docs, architecture decisions, repo scaffolding, risk reviews.
* **Codex CLI**: implementation inside the repo—small diffs, tests/builds, refactors, mechanical edits, CI-fix loops.

Codex should follow AGENTS.md and treat docs/ as authoritative.

### Where to start in docs
Open:
* docs/Canonical_Documentation_Index_v1_0.md
* docs/Bench_Test_Checklist_v1_0.md

### Safety + secret hygiene
Do not commit secrets (Wi-Fi passwords, tokens, private keys). Redact any discovered secrets immediately and rotate them. 
