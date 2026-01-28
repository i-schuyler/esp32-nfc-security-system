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

