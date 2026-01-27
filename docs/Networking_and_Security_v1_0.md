ARTIFACT — networking_security.md (Networking & Security v1.0)

# Networking & Security (V1)

This document defines Wi‑Fi behavior, access control, and the security boundaries.

## 1) Threat Model (Practical, V1)

This is a workshop alarm, not a bank vault.

Assume attacker capabilities:
- Physical presence near the device and NFC reader
- Ability to jam Wi‑Fi or power-cycle device
- Ability to try random NFC tags repeatedly

Non-goals:
- Defending against a motivated attacker with hours of physical access and tools
- Nationwide remote adversaries (no internet requirement)

## 2) Network Modes (Offline-first)

The device operates in one of these modes:

- **STA mode**: joins a known Wi‑Fi network (preferred when available)
- **AP mode**: hosts its own password-protected Wi‑Fi network (fallback)

Rules:
- If known STA credentials exist, attempt STA join on boot for a bounded time.
- If join fails, fall back to AP mode.
- Mode changes are logged and shown in UI.

## 3) AP Mode Requirements

- AP is always password protected (WPA2).
- Default SSID format: `Workshop Security System - A1B2` (suffix from ESP32 MAC last bytes).
- UI must clearly show when you’re connected to AP vs STA.

## 4) Web UI Access Control

Principle: default UI is read-only.

Admin actions require an explicit **Admin Config Mode** session.

### Admin Config Mode (Proposed)
- Entered via authorized NFC admin action when NFC is present.
- Also entered via web-based admin password (Setup Wizard) when NFC is absent/disabled.
- Expires after N minutes of inactivity (configurable).
- While active, UI shows a banner: “Admin Mode Active (expires in …)”.
- All admin actions are logged.

Admin-only actions:
- Change Wi‑Fi SSID/password
- Enter maintenance/test mode
- Change sensor sensitivity/calibration
- OTA firmware update (optional: require admin mode)

**Decision:** which admin actions require the NFC gate vs just a web password.
For V1, the minimum requirement is: sensitive actions require Admin Config Mode, and Admin Config Mode can be entered without NFC via web-based password.

## 5) Changing Wi‑Fi Name/Password

You requested the user can securely set/change SSID/password at any time via web UI.

Contract:
- Requires Admin Config Mode active.
- Device validates new credentials (non-empty, length bounds).
- Device applies changes in a predictable way:
  - If in STA mode: save creds, attempt reconnect, fall back to AP if fail.
  - If in AP mode: restart AP with new SSID/pass after explicit “Apply” click.
- Device logs `config_change` with keys changed (not the secret values).

## 6) Minimal Attack Surface

- No cloud endpoints.
- No unnecessary services.
- Prefer a single HTTP server with:
  - read-only endpoints open
  - admin endpoints blocked unless admin mode active

## 7) OTA Update Security

V1 baseline:
- OTA is local-only via web UI upload of `.bin`.
- Must be recoverable and non-destructive (dual partition preferred).
- Update workflow is fully visible and logged.

Optional hardening (decision):
- checksum display/verification
- signed firmware requirement (V2+)


## 8) Factory Restore (Security Boundary)

Factory restore is a sensitive action.

- Must be Admin-only (Admin Config Mode required).
- Must require explicit confirmation and should also require hold-to-confirm.
- Must be logged (best-effort; if logs are wiped, record a reset marker on next boot).
