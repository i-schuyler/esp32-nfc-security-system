# AGENTS — esp32-nfc-security-system

This repository is a hardware/security firmware project. Changes must preserve safety defaults, avoid drift, and obey canonical docs.

## Authority order
1) `docs/Canonical_Documentation_Index_v1_0.md` (reading order + authority)
2) Existing repo structure and contracts in `/docs`
3) CI/workflows as guardrails
4) Best practices (optional; propose before applying)

## Non-negotiables
- No secrets in repo, logs, or example configs.
- No silent schema changes. Append-only fields/keys; forward-only migrations; log migration events.
- Fail-safe outputs on boot/reset.
- No parallel stacks: extend existing modules; don’t create new competing managers/APIs.

## How to work (Codex)
- Prefer small diffs. One concern per commit.
- Before changing behavior, identify the integration point(s) affected:
  config store, logger/schema, state machine API, web server routes, outputs API, storage manager.
- If a change touches security boundaries (auth, logging, OTA), stop and require explicit confirmation in the PR description.

## CI expectations
- Push/PR must compile via GitHub Actions.
- Artifacts are produced only via manual workflow dispatch or release tags.
- When CI fails due to toolchain/environment issues, run `tools/toolchain_sanity_check.sh` first and include the results in the next change plan.

## Codex Output Contract (required in every response)
Codex must always report:
- Scope: “Slice X only” + explicit “NOT in this slice”
- Touched files list
- Behavior changes (external effect)
- Verification: local skipped if pio absent; CI pending/green/red
- Stop conditions encountered (none/list)
- Re-entry hint: next exact human commands

## Hard Stop Conditions (Codex must STOP and ask)
- Missing API/library/tooling required for the task (e.g., PN532/NDEF not present)
- Canonical docs ambiguous about security boundary/policy
- Any change would violate locked decisions (UID logging, admin-only clear, append-only schema)
- Any schema change not append-only / migration unclear
- Any risk of secret/UID leakage
- Parallel-stack risk (new manager duplicating existing one)
- Safety-adjacent behavior change without explicit confirmation

## Termux / CI Truth
- PlatformIO `pio` is NOT installed on phone/Termux.
- Therefore CI compile-check is the authoritative build verification.
- Codex must not request `pio run` on phone; if build needed locally, say “CI will verify” and keep changes small.

## Human Diff Reality Check (pre-push/PR)
Use these exact commands:
- `git log --oneline main..HEAD`
- `git diff --stat main..HEAD`
Note: `git diff --stat` alone only shows uncommitted changes.

## CI Fix Loop (patch same PR)
If CI fails, fix on the SAME branch/PR with a tiny targeted commit, push, rerun CI. Avoid opening a second PR unless explicitly necessary.

## Slice Workflow Template (standard)
1) Create branch
2) Implement slice
3) One commit with clear message
4) Push + PR
5) CI green
6) Merge + delete branch
7) Pull main
Note: PR text should restate scope + NOT-in-slice.

## Micro-guideline: avoid scope mistakes
- Prefer declaring `now_ms` once per loop and reuse; avoid redeclare/shadow in the same scope (prevents simple CI failures).
