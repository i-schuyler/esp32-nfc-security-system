# Assumptions Registry (hardware/security safe defaults)

Rules:
- Treat unknowns as disabled/safe until confirmed.
- Append-only; if changed, add a new entry that supersedes the old one.

---

## A1 — 2026-01-28 — CI is compile-check by default [LOCKED]
- CI on push/PR compiles only; artifacts are produced only on manual dispatch or release tags.

## A2 — 2026-01-28 — No secrets in repo/logs [LOCKED]
- Never commit or log Wi-Fi passwords, API keys, private keys, tokens, or passphrases.
