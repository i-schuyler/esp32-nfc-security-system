# Decisions Snapshot (append-only)

Rules:
- Append-only. Never rewrite history; add a new entry with [REVERSED] if needed.
- Record only decisions actually made (not ideas).
- Use tags: [LOCKED] / [TENTATIVE] / [REVERSED]

---

## D1 — 2026-01-28 — Repo name locked [LOCKED]
- Repository: esp32-nfc-security-system

## 2026-01-28 — Codex + GitHub CI Foundation

- [LOCKED] Repository name: `esp32-nfc-security-system`
- [LOCKED] Workflow boundary: ChatGPT for planning/specs/docs/architecture; Codex CLI for in-repo implementation and CI-fix loops.
- [LOCKED] CI policy: compile-check on push/PR; artifact-producing builds are optional/manual to keep CI fast.

## D2 — 2026-02-01 — M6 NFC policy locks [LOCKED]
- [LOCKED] `allow_user_silence` default: true (admin may disable).
- [LOCKED] Timestamp-unavailable sentinel for NFC NDEF payloads: `"u"` (unknown); ultra-minimal may omit timestamps.
- [LOCKED] Lockout can be cleared early by a valid Admin scan (and is logged).
- [LOCKED] No NFC writeback on disarm (disarm is logged only).
- [LOCKED] Clear Alarm action requires Admin role.

## D3 — 2026-02-02 — M7.1 Setup Wizard routing locks [LOCKED]
- [LOCKED] Pre-v1 URL changes are allowed; v1 URLs freeze after M8.
- [LOCKED] Setup gating uses existing status flag `setup_required` (no new flags).
- [LOCKED] Setup Wizard is served at `/setup` as a separate HTML page.
- [LOCKED] `/setup` provides a safe escape hatch (no privileged actions).
- [LOCKED] Re-run setup requires Admin Authenticated (not merely Admin Eligible).
