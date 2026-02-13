# Diagnostics Firmware Usage (Board Bring-up)

This document describes how to build and run the diagnostics firmware described in `docs/Diagnostics_Board_Bringup_Firmware_v1_0.md`.

## Build targets

- ESP32 DevKit V1 (esp32dev):
  - `pio run -e diagnostic_esp32dev`
- ESP32-S3 Dev Kit (ESP32-S3-DEV-KIT-N32R16V-M):
  - `pio run -e diagnostic_esp32s3`
  - NOTE: `esp32-s3-devkitc-1` board id is [TENTATIVE] for this hardware; confirm if CI or flashing fails.

## Serial monitor

- `pio device monitor -b 115200`

## Interactive prompts

- SD erase/format:
  - Default is NO. The prompt requires typing `ERASE` to proceed.
  - The prompt is destructive and intended only for diagnostics.
- PN532 UART test:
  - The diagnostic firmware prompts for RX/TX pins.
  - For ESP32-S3, the default uses board UART defaults when pins are left unset; enter explicit pins if required by wiring.

## Factory restore (best-effort)

- Erases NVS and formats LittleFS when present.
- Does NOT touch eFuses, MAC, or radio calibration data.
- Does NOT format SD unless explicitly confirmed in the prompt.
