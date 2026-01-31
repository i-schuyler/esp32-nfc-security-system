# Codex CLI Workflow and Safety v1.0

## Purpose
This document defines the safe, predictable, and low-surprise way to use Codex CLI in this repository. The project is safety-adjacent (alarm horn/strobe, human response) and must preserve offline-first operation, deterministic state transitions, and tamper-aware logging.

Codex is treated as a power tool: useful under supervision, hazardous without guardrails.

## Scope
In scope:
- Installing and pinning the Termux Codex fork (LTS).
- Repo-scoped configuration via `.codex/config.toml`.
- Safety guardrails (no silent command execution, no secret leakage, no surprise refactors).
- Suggested prompts and operating patterns for review-first development.

Out of scope:
- Changing security model, NFC rules, logging schema, or state machine semantics without canonical doc alignment and change-control.

## Installation Strategy
Codex CLI is installed once globally in Termux and used across repositories. Repository behavior is controlled via repo-scoped configuration.

Recommended for this repository:
- Termux fork **LTS** line to reduce surprise and breakage risk.
- Exact version should be pinned once confirmed stable.

## Configuration Strategy
### Repo-scoped policy: `.codex/config.toml`
This repository uses a repo-scoped configuration file to enforce safety defaults.

Required properties:
- `approval_policy = "untrusted"`: commands require explicit approval.
- `sandbox_mode = "read-only"`: prevents silent edits by default.
- `forced_login_method = "chatgpt"`: uses ChatGPT sign-in (no API key required).
- `check_for_update_on_startup = false`: avoids update surprises mid-session.

Secrets must never be stored in `.codex/config.toml` or committed files.

### User-scoped configuration
User-scoped settings (if any) belong in the user’s Termux home environment and must not contain project secrets or credentials intended for the repository.

## Safety Model
### Guardrail 1: Read-only default
Codex sessions should start in read-only mode. This allows inspection, tracing, and planning without changing code.

Escalation to write mode should be explicit, temporary, and scoped to a small change.

### Guardrail 2: Untrusted command approval
Codex must not run shell commands freely. Commands are reviewed and approved intentionally.

High-risk commands (examples): filesystem deletes/moves, mass format changes, dependency upgrades, anything that modifies schema or logs.

### Guardrail 3: No silent drift
Codex proposals are treated as drafts until applied. Any change that affects behavior must be aligned with canonical docs or routed through change-control.

### Guardrail 4: Secrets and privacy
No secrets may be pasted into Codex prompts, committed files, logs, or `.codex/` configuration. If a secret is encountered, it must be redacted as `[REDACTED]` and handled outside the repo.

## Suggested Operating Patterns
### Review-first (recommended default)
1. Ask Codex to locate relevant integration points and constraints.
2. Ask for a minimal patch plan (files touched, risks, tests).
3. Only then allow a small change.

Example prompts:
- “Identify the integration points involved in NFC auth and state transitions, and list files to review. Do not propose changes yet.”
- “Given the canonical docs in `/docs`, propose a minimal change plan for X. Include risks, touched files, and tests. No code edits.”

### Small-diff implementation
When code edits are allowed:
- Keep diffs small and local.
- Avoid sweeping refactors.
- Preserve logging, state machine determinism, and fail-safe defaults.
- Update docs in the same PR when behavior changes.

## “Questions to Ask” Checklist
### 1) What is the authority source for this change?
Answer: canonical docs in `/docs` and existing repo contracts win. If a change conflicts, use change-control (quote, anchored location, replacement text).

### 2) Which security boundary does this touch?
Answer: NFC auth, web UI auth gates, and logging/evidence are boundaries. Changes must be explicit and reviewed.

### 3) Does this change introduce surprise state transitions?
Answer: the state machine must remain explicit; no silent arming/disarming, no hidden transitions. All transitions must be logged.

### 4) Does this change weaken fail-safe behavior?
Answer: on uncertainty, prevent false-disarm and preserve logs. If SD/RTC are missing, degrade gracefully with loud status.

### 5) Does this change touch logging schema?
Answer: schema changes are append-only. Renames/removals require migrations and must be logged.

## Troubleshooting Notes (Termux)
- Verify active binary: `which codex` and `codex --version`.
- Prefer LTS for stability; pin exact versions once stable.
- If Codex behaves unexpectedly, confirm `.codex/config.toml` is being detected and that sandbox/approval match expectations.

## Re-entry Hint
When returning to a session:
1. Confirm `codex --version`.
2. Confirm `.codex/config.toml` exists and contains read-only + untrusted settings.
3. Start in review-first mode and request a minimal patch plan before allowing edits.
