ARTIFACT — nfc_contracts.md (NFC Data Contracts v1.0)

# NFC Data Contracts (V1)

This document defines:
- NFC roles and permissions
- On-tag data written by the system
- Incident writeback format
- Anti-abuse behavior and lockouts

## 1) Tag Types (Assumption-free)

We will treat tags as **Type 2** (NDEF-capable) unless you decide otherwise.

V1 will support multiple capacities (e.g., NTAG213/215/216). The device detects available NDEF capacity at write time and may write a minimal/truncated record on smaller tags (contract below).

## 2) Roles

Recommended minimum:
- **Admin**: arm/disarm, clear alarm, enter maintenance/test mode, change config (including Wi‑Fi creds)
- **User**: arm/disarm, optionally silence (if enabled), cannot change configuration

## 3) Permissions Matrix (Proposed)

| Action | Admin | User |
|---|---:|---:|
| Arm | ✅ | ✅ (configurable) |
| Disarm | ✅ | ✅ (configurable) |
| Silence | ✅ | ✅ (configurable) |
| Clear triggered alarm | ✅ [LOCKED] | ❌ |
| Enter maintenance/test mode | ✅ | ❌ |
| Change Wi‑Fi creds | ✅ | ❌ |
| Add/remove cards | ✅ | ❌ |

All of the above are configurable, but the defaults should minimize surprise.

## 4) Anti-Abuse Rules

- Rate-limit invalid scans.
- After N invalid scans within W seconds → lockout for L seconds.
- All scans are logged (including those ignored due to lockout).
- A valid Admin scan may clear lockout early; the clear action is logged and the scan is processed normally.

## 5) On-Tag Data Written by Device

**Overwrite rule (V1):** When the device writes to a tag, it clears and rewrites the entire NDEF message so the tag always reflects the latest state/incident. No incremental append.

Two separate writebacks requested:
1) **Armed state written when user arms the system**
2) **Incident summary written when clearing a triggered alarm**
3) **No writeback on disarm** (disarm is logged only)

Clear Alarm writeback gating:
- When an Admin attempts Clear Alarm, the system attempts incident writeback first.
- If the writeback fails, the clear is rejected (no state change) and the failure is logged.

### Privacy rule
Never store secrets on the tag. Store only operational summaries.


### Optional URL record (phone tap)
- When `nfc_url_record_enabled=true`, the device writes a URL NDEF record intended to open the local web UI.
- On every writeback, firmware rewrites the entire NDEF message and preserves the URL record alongside the current system record when capacity allows.
- If capacity is too small, the system record takes priority and the URL record may be omitted; this omission must be logged.

Recommended V1 behavior (`nfc_url_mode`):
- `ap` (default): URL record is `http://192.168.4.1/` (works reliably in AP mode).
- `mdns`: URL record is `http://workshop-security-system-A1B2.local/` (device-hostname based).
- `custom`: URL record is an administrator-supplied value (e.g., fixed LAN IP or a preferred hostname).

Notes:
- The Setup Wizard should offer a simple choice: AP default (recommended) vs Custom URL.
- If `mdns` is selected, the device hostname must be stable and derived from the device ID; mobile mDNS support varies by network and platform, so AP default remains the safest default.
### 6.1 Optional: Phone-tap URL record (nice-to-have V1)

If `nfc_url_record_enabled=true`, the NDEF message may include a standard URL record intended to open the local web UI when tapped by a phone.

- Default URL (AP mode): `http://192.168.4.1/`
- If installed on a LAN, the URL may be overridden in config (e.g., a fixed IP or mDNS hostname).
- On every writeback, firmware rewrites the entire NDEF message and **preserves the URL record** alongside the current system record when capacity allows.
- If capacity is too small, the system record takes priority and the URL record may be omitted; this omission must be logged.

### 6.2 System record name (based on project title)

For maximum interoperability and stability, the system record should use an **NFC Forum External Type**.

**Recommended record type (V1):**
- `urn:nfc:ext:esp32-nfc-security-system:v1`

Rationale:
- It is derived from the GitHub project title (stable technical identifier).
- It avoids spaces and punctuation that can cause interoperability problems.
- It supports future record evolution (`v2`, `v3`, etc.) without ambiguity.

(If a more product-forward identifier is preferred, `urn:nfc:ext:workshop-security-system:v1` also works, but the repo title is recommended as the canonical type.)

### 6.3 Encoding

- Payload is UTF‑8 JSON.
- JSON is written in **compact form** (no whitespace).
- On smaller tags, a defined **minimal JSON form** is used (short keys, fewer fields).

### 6.4 Record variants

Two `type` values are defined in V1:

1) `armed` — written on successful arm
2) `incident` — written when clearing a triggered alarm

### 6.5 Full field set (preferred, human-readable)

Timestamp validity:
- When time is valid, timestamps are ISO-8601 strings.
- When time is unavailable, timestamp fields are set to `"u"` (unknown); ultra-minimal may omit timestamps.

Example `armed` payload:
```json
{"v":1,"type":"armed","ts":"2026-01-23T19:46:12-08:00","state":"ARMED","device":"esp32-A1B2","role":"user"}
```

Example `incident` payload:
```json
{"v":1,"type":"incident","trigger_ts":"2026-01-23T20:01:05-08:00","clear_ts":"2026-01-23T20:04:11-08:00","source":"motion","duration_s":186,"cleared_by":"admin","device":"esp32-A1B2"}
```

### 6.6 Minimal field set (deterministic truncation target)

When capacity is constrained, the system must write a minimal payload with short keys.

Minimal `armed` payload:
```json
{"v":1,"t":"a","ts":"2026-01-23T19:46:12-08:00","s":"A","d":"A1B2"}
```

Minimal `incident` payload:
```json
{"v":1,"t":"i","tt":"2026-01-23T20:01:05-08:00","ct":"2026-01-23T20:04:11-08:00","src":"m","cb":"a","d":"A1B2"}
```

Key mapping (minimal form):
- `t`: record type (`a`=armed, `i`=incident)
- `s`: state (`A`=ARMED, `D`=DISARMED, `T`=TRIGGERED, `S`=SILENCED, `F`=FAULT)
- `d`: device suffix (ESP32 MAC last bytes or configured short ID)
- `tt`: trigger timestamp
- `ct`: clear timestamp
- `src`: source (`m`=motion, `d`=door, `t`=tamper, `p`=power/other)
- `cb`: cleared-by role (`a`=admin, `u`=user)

### 6.7 Capacity detection + byte budgets

V1 supports multiple tag capacities. Capacity is detected at provisioning time and recorded in device config for that tag (and re-validated on write if desired).

Definitions:
- `C`: detected maximum NDEF message bytes writable to the tag
- `R_url`: reserved bytes for URL record if enabled (recommended reserve: 48 bytes)
- `C_sys`: bytes available for the system record and its NDEF overhead

Policy:
1) Compute `C_sys = C - (R_url if url enabled else 0)`.
2) Attempt to write **Full** payload.
3) If it does not fit, write **Minimal** payload.
4) If Minimal still does not fit, write an **Ultra-minimal** payload (see 6.8) and log `nfc_truncate_ultra=true`.
5) If even Ultra-minimal does not fit, log failure and do not change system state based on the writeback result.

Recommended payload budgets (excluding NDEF record overhead; conservative targets):
- Tier S (small, e.g., NTAG213): Full ≤ 120 bytes; Minimal ≤ 96 bytes
- Tier M (medium, e.g., NTAG215): Full ≤ 360 bytes; Minimal ≤ 160 bytes
- Tier L (large, e.g., NTAG216): Full ≤ 720 bytes; Minimal ≤ 240 bytes

(Exact `C` differs by tag/chip. V1 treats `C` as authoritative and uses the budgets above as safe targets.)

### 6.8 Ultra-minimal fallback (last resort)

If needed, the device writes a tiny payload that still preserves meaning:

- Armed:
```json
{"v":1,"t":"a","s":"A","d":"A1B2"}
```

- Incident:
```json
{"v":1,"t":"i","src":"m","cb":"a","d":"A1B2"}
```

This fallback prioritizes: version, type, device ID, and minimal semantics.

## 7) Fixed-Size Overwrite Contract (Mandatory fields)

On every writeback, the tag’s NDEF message is rewritten.

Mandatory rules:
- The system record is always overwritten (never appended).
- If URL record is enabled, it is preserved **only if** the system record can still be written successfully.

Mandatory fields (Full form):
- Always: `v`, `type`, `device`
- Armed must include: `ts`, `state`
- Incident must include: `trigger_ts`, `clear_ts`, `source`, `cleared_by`
- `duration_s` is recommended but optional (derivable from timestamps)

Mandatory fields (Minimal form):
- Always: `v`, `t`, `d`
- Armed must include: `s` (and `ts` if capacity allows)
- Incident must include: `src`, `cb` (and `tt`/`ct` if capacity allows)

If truncation occurs:
- A log event is written including which tier was used (Full/Minimal/Ultra-minimal).
- UI indicates “NFC writeback truncated” for the most recent writeback.

## 8) Card Provisioning (V1 Supported)

Provisioning is supported in V1.

Provisioning rules:
- No “magic learning” outside explicit provisioning mode.
- Provisioning is initiated from the guided setup flow or Admin Config Mode in the web UI.
- Each provisioning session is logged from start to finish.
- Provisioning times out automatically (configurable) and returns to normal operation.

Supported flows:
- Add Admin tag
- Add User tag
- Remove tag
- List tags (admin-only; identifiers should be privacy-preserving)

Recommended safeguards:
- Require hold-to-confirm for entering provisioning mode.
- Require a second confirmation click in UI for destructive actions (remove/factory reset).


## 9) “Long Tap” Gesture (Optional)

If you approve:
- Hold tag steady for X seconds to confirm sensitive actions (enter admin mode, clear alarm).
This is implemented by repeatedly detecting the same UID continuously.

This is a decision item (see Decisions doc).
