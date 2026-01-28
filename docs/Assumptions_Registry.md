# Assumptions Registry (hardware/security safe defaults)

Purpose: Make assumptions explicit. Unknowns default to safe/disabled behavior.

Rules:
- Append-only.
- If an assumption becomes false, mark it [REVERSED] and add a replacement.

---

## A1 — Unknown pins default to disabled
Status: [LOCKED]
Assumption:
- If pin mapping is not explicitly configured, related features remain DISABLED and report that status.

## A2 — Secrets never stored in logs or NFC tags
Status: [LOCKED]
Assumption:
- Wi-Fi credentials, tokens, keys, and admin secrets are never logged and never written to NFC tags.

## A3 — CI initially compiles only
Status: [TENTATIVE]
Assumption:
- CI’s first job is compile-check; artifact publishing is added after stability is proven.

## A4 — Unknown auth state defaults to deny
Status: [LOCKED]
Assumption:
- If authentication/authorization state is ambiguous, the system denies privileged actions and remains in a safe state.
