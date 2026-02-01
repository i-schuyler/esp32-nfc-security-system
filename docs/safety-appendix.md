# Safety Appendix (Public Release, Read-Only)

This appendix summarizes the security boundaries, operational assumptions,
and safety defaults for a low-attention public release.

## Security boundaries

- **NFC boundary**: NFC credentials authorize arm/disarm and admin actions.
- **Web UI boundary**: the default UI is read-only; admin actions require
  explicit Admin Config Mode.

These are the only trusted boundaries for control actions.

## Offline-first assumptions

- The system is designed to operate without internet access.
- STA join is attempted when credentials exist; AP mode is the fallback.
- Mode changes are visible in UI and logged.

## Fail-safe defaults

- Outputs default to safe states on boot/reset.
- Configuration defaults prioritize safety and explicit user action.
- Dangerous actions require Admin Config Mode and explicit confirmation.

## Log and secret hygiene

- Logs record configuration changes and security-relevant events without
  storing secret values.
- Credentials and private keys are treated as secrets; do not place them
  in logs, example configs, or issues.

## Public threat model

Assume attackers can read the code and documentation.

Non-goals include defending against a determined attacker with prolonged
physical access. The device remains a local, workshop-focused system with
no cloud dependencies.
