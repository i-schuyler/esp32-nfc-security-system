# Diagnostics Firmware Usage (Board Bring-up)

This document describes how to build and run the diagnostics firmware described in `docs/Diagnostics_Board_Bringup_Firmware_v1_0.md`.

## Build targets

- ESP32 DevKit V1 (esp32dev):
  - `pio run -e diagnostic_esp32dev`
- ESP32-S3 Dev Kit (ESP32-S3-DEV-KIT-N32R16V-M):
  - `pio run -e diagnostic_esp32s3`
  - NOTE: `esp32-s3-devkitc-1` board id is [TENTATIVE] for this hardware; verify with `pio boards espressif32` if CI or flashing fails.

## Serial monitor

- `pio device monitor -b 115200`

## Interactive prompts

- SD erase/format:
  - Default is NO. The prompt requires typing `ERASE` to proceed.
  - The prompt is destructive and intended only for diagnostics.
- PN532 UART test:
  - The diagnostic firmware prompts for RX/TX pins.
  - For ESP32-S3, defaults are RX=44 and TX=43 (TXD/RXD header pins). The chosen pins are printed before the test.
  - Enter explicit pins at the prompt to override defaults if wiring differs.

## If PN532 test fails

- Confirm PN532 power and GND are correct.
- Swap RX/TX if wired incorrectly.
- Enter explicit RX/TX pin numbers at the prompt.
- Confirm baud rate matches the PN532 HSU setting (default 115200).

## Factory restore (best-effort)

- Erases NVS and formats LittleFS when present.
- Does NOT touch eFuses, MAC, or radio calibration data.
- Does NOT format SD unless explicitly confirmed in the prompt.
