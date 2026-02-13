ARTIFACT — diagnostics_firmware.md (Board Bring-up Diagnostics v1.0)

# Board Bring-up Diagnostics Firmware (V1)

Purpose:
- Provide a standalone diagnostics firmware for board bring-up and manufacturing checks.
- This firmware is **NOT** the customer product and must never be shipped as production firmware.

## Scope and Non-Scope

In-scope:
- Hardware sanity checks (PSRAM, flash, SD, I2C, UART, GPIO probe).
- Best-effort factory restore for known storage tiers.

Out-of-scope:
- Any customer features (state machine, NFC access control, web UI flows, OTA updates).
- Any permanent hardware changes (eFuses, MAC, radio calibration data).

## Factory Restore (best-effort) definition

When invoked, the diagnostics firmware performs a **best-effort** restore with these rules:
- Erase NVS (configuration storage).
- Format SPIFFS/LittleFS if present.
- **Do NOT** touch eFuses, MAC address, or radio calibration data.
- **Do NOT** format SD unless explicitly requested by the operator.

## Test Suite Checklist

Each test must report PASS/FAIL with a short reason.

1) PSRAM test (if PSRAM present)
   - Allocate a buffer, write known patterns, read back and verify.
   - Report the largest contiguous allocation tested.

2) Flash partition sanity
   - Enumerate partitions (name, type, subtype, size).
   - Perform a basic read/CRC check on each partition where safe.

3) SD card test
   - Initialize SD.
   - Create a temp file, write data, read back and verify, then delete.
   - Optional destructive step (interactive):
     - Default behavior: do NOT erase/format the SD.
     - Prompt via Serial terminal to confirm SD erase/format; default response is NO.
     - Require explicit confirmation (for example, type `ERASE` or use Y/N plus a second confirm).
     - Log/print that SD erase is destructive and intended only for diagnostics.
     - Log/print only actions and results (no secrets).

4) I2C scan + DS3231 detect
   - Scan the bus for devices.
   - Report DS3231 presence (address 0x68).

5) UART test mode (PN532)
   - Attempt a PN532 handshake and firmware version read over UART/HSU.
   - Report the PN532 firmware version when available.

6) Pin probe toggle test
   - Toggle a user-selected list of GPIOs and confirm output state changes.
   - Enforce a denylist (board-specific) to avoid reserved pins.

## Output expectations and failure signals

- Provide a concise serial log with:
  - Firmware version and board target
  - Each test name, PASS/FAIL status, and error reason
  - A final summary line with overall status
- Failure signals should include:
  - A stable error code string (for automated parsing)
  - The failing subsystem (psram/flash/sd/i2c/uart/gpio)

## Integration in repo

- Implemented as a separate build target (separate PlatformIO env or separate binary).
- Must not share the production firmware entrypoint or state machine.
- Must be explicitly invoked; never enabled in customer builds.
