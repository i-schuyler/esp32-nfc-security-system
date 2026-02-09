# Web UI Style + Language Spec — Mode C (Guided Operator)

## 1) UI stance (Mode C)
Primary intent: help an operator succeed on a phone screen, offline, under stress, without surprises.

Pillars:
- Calm: no flashing UI, no noisy language.
- Explicit: always show what state the system is in, why, and what will happen next.
- Guided: short prompts that prevent wrong actions.
- Durable: UI reinforces evidence/logs as first-class.

Approved top tagline:
“Calm system. Clear state. Durable logs.”

## 2) Visual style (no external assets)
Layout:
- Single column, mobile-first.
- Sections in order: Status → Recent Events → Controls → Config → Diagnostics → Maintenance.
- Each section is a card with: title row; status rows (label left, value right); actions at bottom.

Typography:
- System font only.
- Size scale: Title 18–20; Section header 16–18; Body 14–16; Helper 12–14.
- Line height: roomy for mobile readability.

Color/affordance rules:
- Neutral base.
- Semantic accents only: Green=ok, Amber=attention/unknown, Red=danger/triggered/fault.
- No gradients; no animations beyond subtle loading indicators.

Buttons (hierarchy):
- Primary: the one main action in a view (rare).
- Secondary: safe actions (refresh/view/exit).
- Danger: destructive/irreversible (factory restore, clear alarm, OTA reboot). Always requires confirmation.

## 3) Copy rules (hard constraints)
Must:
- Use plain language.
- Include consequences on dangerous actions.
- Use the same term everywhere (see lexicon).
- Prefer “Unknown” over showing sentinels (never show "u").

Must not:
- No jokes.
- No vague verbs (“fix”, “handle”, “do it”).
- No hidden transitions.
- No blaming language in errors.

## 4) Lexicon (canonical labels)
States (display exactly):
- DISARMED
- ARMED
- TRIGGERED
- SILENCED
- FAULT

NFC roles (display):
- Admin
- User
- Unknown (never show UID)

Terms:
- Events = log entries shown in UI
- Logs = downloadable text files
- Admin Eligible = NFC-enabled window is open
- Admin Authenticated = password session active
- Setup Wizard = guided setup flow

## 5) Status section — exact fields + guided phrasing
Status card fields:
- State: <STATE>
- Last change: <reason> — <timestamp or Unknown>
- Time: Valid / Unknown
  Helper (if unknown): “Timestamps may be unavailable until RTC is set.”
- Network: AP or STA; show IP; show SSID if STA
- Storage: SD OK / SD Missing (Using Flash Fallback)
- NFC: OK / Unavailable / Degraded; show “Provisioning enabled: Yes/No”
- Lockout: None / Active (<remaining>)
- Admin mode:
  - Admin: Off
  - Admin: Eligible (<remaining>)
  - Admin: Authenticated (<remaining>)

Guided helper line (only when relevant):
- If TRIGGERED: “Alarm is latched until cleared by Admin.”
- If FAULT: “Some guarantees may be reduced. Review faults below.”

## 6) Admin gating UX (additive NFC enable + password)
Behavior:
- When setup is complete, at least one Admin card exists, and the NFC reader is healthy: an Admin NFC scan opens the eligibility window (time-limited).
- During setup (setup not complete), or before the first Admin card exists, or when the reader is unhealthy: allow admin password entry in the Setup Wizard without an eligibility window; copy should state this is for first Admin card bootstrap.
- Password still required to become Admin Authenticated.

UI presentation banner (only when eligible/authenticated):
When Admin Eligible:
- Title: “Admin enabled (scan accepted)”
- Body: “Enter the admin password to make changes.”
- Countdown: “Expires in: mm:ss”
- Buttons: “Enter password” and “Cancel” (closes eligibility early)

When Admin Authenticated:
- Title: “Admin authenticated”
- Body: “Changes are logged.”
- Countdown + “Exit admin mode”

Login copy:
- Header: “Admin password”
- Helper (normal): “Admin access requires a recent Admin NFC scan.”
- Helper (setup bootstrap): “Enter the admin password to add the first Admin card.”
- Errors:
  - “Admin window expired. Scan an Admin card again.”
  - “Incorrect password.”

## 7) Confirmations (two-step standard)
For any Danger action:
1) First tap shows inline confirm panel:
   - “This will <consequence>.”
   - Buttons: “Confirm” (danger) + “Cancel”
2) Optional hold-to-confirm only for the most destructive actions.

Danger actions in V1:
- Clear Alarm
- Factory Restore
- OTA Update
- Delete logs (if offered)

## 8) Controls / Test mode text
Controls section (read-only by default):
- If not admin: “Controls are available in Admin mode.”

Test Mode (Admin only):
- Title: “Output tests (non-security)”
- Helper: “Tests do not arm or disarm the system. Tests stop automatically.”
- Buttons: “Test horn (5s)”, “Test light (5s)”, “Stop tests”

## 9) Recent Events + Logs download copy
Recent Events:
- Title: “Recent events”
- Filter label: “Filter”
- Helper: “Events are append-only.”

Download logs:
- Title: “Download logs”
- Buttons: “Today”, “Last 7 days”, “All (may be limited)”
- If limited/refused: “Too large to download. Choose a shorter range.”

## 10) OTA section copy (Admin only)
- Title: “Firmware update”
- Helper: “Uploading firmware writes to the inactive partition and reboots on success.”
- After upload fields: File, Size, Result
- On success: “Rebooting now. Reconnect to the device Wi-Fi.”

## 11) Wizard tone (Mode C)
Each step uses: one sentence (what/why), one instruction (next), one status line (detected).
Example Time & RTC step:
- “Accurate timestamps improve logs and incident history.”
- “Set timezone and confirm RTC is detected.”
- “RTC: Detected / Not detected”

# NFC additive admin mode (M7 decision)
- Admin NFC scan opens Admin Eligible window only when setup is complete, at least one Admin card exists, and the NFC reader is healthy.
- During setup, admin password entry in the Setup Wizard can start Admin Authenticated for first-card bootstrap (no eligibility window).
- Password still required for Admin Authenticated.
- UI distinguishes Eligible vs Authenticated with countdowns.
- No raw UID displayed or logged.
