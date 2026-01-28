# Drift Ledger

Purpose:
- Track mismatches between docs, README, workflows, and code reality.
- Each entry should include: symptom, impact, proposed fix, and status.

---

## DL1 — 2026-01-28 — README is outdated [TENTATIVE]
- Symptom: README does not match current repo structure/spec.
- Impact: onboarding confusion + wrong mental model.
- Proposed fix: rewrite README to match canonical docs + current structure.
- Status: open

## 2026-01-28 — Drift Notes and Resolutions

- Drift: README referenced outdated layout (e.g., UI/firmware folder expectations).  
  Resolution: README updated to reflect current repo structure (`docs/`, `src/`, `data/`, `tools/`, `AGENTS.md`, CI workflow).

- Drift: Termux GH CLI quirks during PR workflow.  
  Notes:
  - `gh api` with `-f` implies form data and can default into POST semantics; prefer `-X GET` + querystring for listing resources.
  - `gh pr create` may fail in some Termux environments with git exec permission errors; use `gh api` or GitHub web UI as fallback.

