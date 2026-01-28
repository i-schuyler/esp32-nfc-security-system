# Drift Ledger

Purpose: Track drift risks (doc/code mismatch, duplicated stacks, unclear authority) and their resolution status.

---

## DL-001 — README repo layout mismatch
Date: 2026-01-28
Signal:
- README describes folders (firmware/, ui/, tools/) that do not match current repo tree.
Risk:
- Humans/CI/agents assume incorrect working directories and structure.
Status: OPEN
Next action:
- Update README to reflect the actual PlatformIO root layout (src/, include/, data/, docs/).

---

## DL-002 — CI workflow invalid + wrong working directory
Date: 2026-01-28
Signal:
- Workflow file contained invalid YAML and pointed at a non-existent firmware/ directory.
Risk:
- CI fails; confidence and feedback loop break.
Status: IN PROGRESS
Next action:
- Replace workflow with compile-check at repo root using PlatformIO.
