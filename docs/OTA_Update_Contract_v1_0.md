ARTIFACT — ota_contract.md (OTA Update Contract v1.0)

# OTA / Firmware Update Contract (V1)

## 1) Requirements

- OTA happens via local web UI upload of a `.bin`.
- No auto-updates.
- UI shows:
  - current firmware version
  - uploaded file name + size
  - update result (success/fail)
  - optional checksum display (decision)

## 2) Safety

- Prefer dual-partition OTA so failed updates can roll back.
- On update start: log `ota_update` event (started).
- On success/failure: log `ota_update` event with result.

## 3) Backwards Compatibility (V1.x)

Within major version v1:
- `/api/status` must remain compatible (fields can be added, not removed/renamed).
- Log schema remains compatible (new optional fields allowed).
- Config schema migrations must be automatic or explicitly blocked with clear UI error.

## 4) Recovery

If OTA fails:
- device should remain reachable (AP fallback)
- device should not silently brick
