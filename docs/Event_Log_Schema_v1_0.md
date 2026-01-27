ARTIFACT — log_schema.md (Event Log Schema v1.0)

# Event Log Schema (V1)

Logs are first-class evidence. This doc defines file layout, rotation, event fields, and required minimum events.

## 1) Storage Tiers

### Tier A — microSD (Preferred)
- Append-only text logs on SD.
- Daily rotation: one file per day.

### Tier B — On-flash ring buffer (Fallback)
- Small ring buffer in flash when SD missing/unavailable.
- UI must show “SD missing” prominently.
- When SD returns, ring buffer can be exported/merged (optional).

## 2) File Layout (SD)

Proposed:
- `/logs/YYYY/`
- `/logs/YYYY/MM/`
- `events_YYYY-MM-DD.txt` (primary)
- `incidents_YYYY-MM-DD.txt` (optional separate incident summaries)

**Decision:** single file vs split files.

## 3) Log Line Format

Human-readable but machine-parseable.

**Canonical format: one JSON object per line (JSONL).**

Example (one line):
```json
{"ts":"2026-01-23T19:46:12-08:00","event_type":"state_transition","from":"DISARMED","to":"ARMED","reason":"nfc_arm","card_role":"admin","card_id":"taghash:abcd...","seq":184}
```

Notes:
- JSONL makes it easy to stream, download, and parse.
- `card_id` should be **non-reversible** (hashed/salted) if you don’t want raw UIDs stored.

## 4) Required Fields (All Events)

- `ts` (ISO 8601 with timezone; RTC preferred)
- `seq` (monotonic sequence number; persists across boots if possible)
- `event_type` (string enum)
- `severity` (debug/info/warn/error/critical)
- `source` (firmware subsystem: nfc/sensor/power/wifi/sd/rtc/ui)
- `msg` (short human summary)

## 5) Common Optional Fields

- `state` (current state after event)
- `from`, `to`, `reason` (for transitions)
- `sensor` fields: `sensor_type`, `sensor_id`, `raw`, `calibrated`, `threshold`
- `power` fields: `mains`, `battery_v`, `low_batt`
- `wifi` fields: `mode` (AP/STA), `ssid`, `rssi`, `ip`
- `fault` fields: `fault_code`, `fault_detail`

## 6) Minimum Required Event Types

- `boot` (reset reason, firmware version)
- `time_status` (RTC ok/missing; time valid)
- `sd_status` (mounted/unmounted; capacity; errors)
- `wifi_mode_change` (AP/STA, reason)
- `state_transition` (all transitions)
- `nfc_scan` (valid/invalid; role; action result)
- `lockout` (entered/exited)
- `sensor_trigger` (source + useful raw data)
- `tamper_event` (enclosure open, reader disconnect, motion tamper if used)
- `ota_update` (started/finished; result; version)
- `config_change` (who/when/what keys changed)

## 7) Rotation + Retention

- New file each day.
- UI can download:
  - today
  - last 7 days
  - “all” (zipped) if feasible
- Optional retention policy: delete logs older than N days when SD nearly full.

**Decision:** retention behavior and thresholds.

## 8) Tamper Awareness
V1 goal: “tamper-aware”, not “cryptographically provable”.

V1 decision:
- Hash chaining is included for SD-backed logs and is enabled by default.
- A configuration key may disable hash chaining for performance or simplicity.

Mechanism (JSONL):
- Each event includes `prev_hash` and `hash` fields when enabled.
- `hash` is computed over the canonical serialized JSON line (excluding `hash` itself), plus `prev_hash`.
- The first line of each daily file uses a fixed `prev_hash` value (e.g., all zeros) and includes a `file_header` event with device identity and schema versions.

Scope:
- Hash chaining is a tamper-evidence aid only; it does not prevent deletion or replacement of entire files.
