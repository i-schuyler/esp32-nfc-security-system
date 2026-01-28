# Decisions Snapshot (append-only)

Purpose: A compact, authoritative record of decisions actually made.

Rules:
- Append-only (do not rewrite history; add a new entry if a decision changes).
- Tag each decision: [LOCKED] / [TENTATIVE] / [REVERSED]
- Prefer dates and concrete wording.

---

## 2026-01-28 — Transition to Codex + GitHub CI
- [LOCKED] Repo name is `esp32-nfc-security-system`.
- [LOCKED] ChatGPT is used for planning/specs/docs/architecture/risk reviews; Codex CLI is used for implementation diffs/tests/refactors/CI-fix loops.
- [LOCKED] Project transitions from zip-tennis to GitHub repo + CI; M6–M8 are out of scope until transition completes.
- [TENTATIVE] CI starts as compile-check only; artifact publishing is added after CI is stable.
- [TENTATIVE] Codex enablement phases: Local CLI → GitHub PR review → CI autofix (each unlocked only after stability triggers are met).
