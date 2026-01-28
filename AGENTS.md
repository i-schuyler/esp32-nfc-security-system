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

