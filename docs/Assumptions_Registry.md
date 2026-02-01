# Assumptions Registry (hardware/security safe defaults)

Rules:
- Treat unknowns as disabled/safe until confirmed.
- Append-only; if changed, add a new entry that supersedes the old one.

---

## A1 — 2026-01-28 — CI is compile-check by default [LOCKED]
- CI on push/PR compiles only; artifacts are produced only on manual dispatch or release tags.

## A2 — 2026-01-28 — No secrets in repo/logs [LOCKED]
- Never commit or log Wi-Fi passwords, API keys, private keys, tokens, or passphrases.

## A3 — 2026-02-01 — NFC absence/degraded defaults to safe [LOCKED]
- If NFC hardware is absent/disabled/disconnected, NFC actions are disabled.
- System remains operable via web UI controls (subject to Admin Config Mode).
- UI and logs must make NFC status explicit.

## A4 — 2026-02-01 — Allowlist identifiers are non-reversible [LOCKED]
- Raw tag UIDs are never written to logs or storage as identifiers.
- Allowlist stores a non-reversible identifier (e.g., salted hash) and role.

## A5 — 2026-02-01 — NFC writeback is best-effort and non-authoritative [LOCKED]
- NFC writeback success/failure never changes the security state outcome.
- Writeback failures/truncation are logged and visible in the UI.

## A6 — 2026-02-01 — Lockout is bounded, visible, and admin-clearable [LOCKED]
- Lockout is time-bounded and visible in UI.
- A valid Admin scan may clear lockout early (and the action is logged).
