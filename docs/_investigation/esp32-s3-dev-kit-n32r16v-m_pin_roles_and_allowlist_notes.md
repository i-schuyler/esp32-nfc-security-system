# ESP32-S3-DEV-KIT-N32R16V-M Pin Roles and Allowlist Notes

Status: Investigation note (docs-only). This file does not change firmware behavior.

## Sources in this repo
- Pin defaults and compile-time map: `src/config/pin_config.h`
- Config defaults: `src/config/config_store.cpp`
- PN532 driver (SPI/I2C behavior): `src/nfc/nfc_reader_pn532.cpp`
- SD storage SPI pins: `src/storage/storage_manager.cpp`
- RTC I2C fallback pins: `src/storage/time_manager.cpp`
- UI safe pin lists and defaults: `data/app.js`
- DevKit V1 wiring map: `docs/Wiring_Instructions_DevKitV1_v1_0.md`
- DevKit V1 allowlist guidance: `docs/Configuration_Registry_v1_0.md`

## Critical pin roles (current DevKit V1 assumptions vs S3 notes)

| Role | Current DevKit V1 assumption (repo source) | ESP32-S3-DEV-KIT-N32R16V-M notes |
| --- | --- | --- |
| LD2410B UART RX/TX | Defaults RX=16, TX=17 in `data/app.js` and `src/config/config_store.cpp`; wiring doc uses GPIO16/17. | Needs two UART-capable GPIOs; candidate pins from photo list only [TENTATIVE]. Must avoid strapping/USB/JTAG/reserved pins [TENTATIVE]. |
| PN532 NFC (SPI by default; I2C optional) | Default interface is SPI (`nfc_interface=spi`) in `src/config/config_store.cpp` and `data/app.js`. SPI bus pins are fixed to 18/19/23 in `src/nfc/nfc_reader_pn532.cpp`; CS/IRQ/RST are configurable and UI-limited via safe pin lists in `data/app.js`. I2C mode requires `WSS_PIN_I2C_*` plus `WSS_PIN_NFC_IRQ/RESET` in `src/nfc/nfc_reader_pn532.cpp`. | Requires SPI pins + CS (and optional IRQ/RST) or I2C + IRQ/RST. S3 pin mapping and safe allowlists are unknown [TENTATIVE]. |
| microSD (SPI) | SPI bus pins fixed to 18/19/23 in `src/storage/storage_manager.cpp`; SD CS default is 13 in `src/config/config_store.cpp` and `data/app.js`. | Requires SPI pins + CS. S3 pin mapping and reserved pins are unknown [TENTATIVE]. |
| DS3231 RTC (I2C) | I2C pins from `WSS_PIN_I2C_*` with fallback to 21/22 in `src/storage/time_manager.cpp`; wiring doc uses 21/22. | Requires I2C-capable pins; S3 mapping unknown [TENTATIVE]. |
| Outputs (horn/light) | Defaults -1 (unset) in `src/config/pin_config.h` and `src/config/config_store.cpp`; UI allowlist is `OUTPUT_GPIO_SAFE_PINS` in `data/app.js`. | Needs two output-capable GPIOs; S3 allowlist is unknown [TENTATIVE]. |
| Inputs (motion/door/enclosure) | Defaults -1 (unset) in `src/config/pin_config.h` and `src/config/config_store.cpp`; UI allowlist is `INPUT_GPIO_SAFE_PINS` in `data/app.js` (includes input-only pins 34-39). | Needs input-capable GPIOs; S3 allowlist is unknown [TENTATIVE]. |

## What the current UI assumes (DevKit V1 oriented)

From `data/app.js`:
- Safe pin allowlists:
  - `LD2410B_SAFE_PINS = [4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33]`
  - `LD2410B_SAFE_TX_PINS = pins < 34`
  - `PN532_CS_SAFE_PINS = [16, 17, 25, 26, 27, 32, 33]`
  - `PN532_RST_SAFE_PINS = PN532_CS_SAFE_PINS`
  - `PN532_IRQ_SAFE_PINS = [32, 33, 34, 35, 36, 39]`
  - `SD_CS_SAFE_PINS = [13, 16, 17, 25, 26, 27, 32, 33]`
  - `OUTPUT_GPIO_SAFE_PINS = [13, 14, 16, 17, 25, 26, 27, 32, 33]`
  - `INPUT_GPIO_SAFE_PINS = [13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39]`
- Wizard defaults:
  - NFC SPI defaults: CS=27, IRQ=32, RST=33; interface is SPI.
  - SD defaults: enabled=true, CS=13.
  - LD2410B defaults: RX=16, TX=17, baud=256000.
  - Outputs and inputs default to -1 (unset).

These lists and defaults are DevKit V1 specific and do not establish an ESP32-S3 allowlist.

## What the S3 board exposes (from user photo)

[TENTATIVE] From the provided photo only; verify against the board datasheet before locking:
- Power/control: 3V3, 5V, GND, RST, TXD, RXD.
- IO labels seen: IO0, IO1-IO18, IO19, IO20, IO21, IO33-IO42, IO45-IO48.

## Conservative docs-only proposed approach

- Keep `docs/Configuration_Registry_v1_0.md` as the source of truth for configuration keys.
- Add per-board allowlists and defaults in a future code slice (no parallel stacks).
- Candidate S3 pin pool from photo list [TENTATIVE]: IO1-IO18, IO19, IO20, IO21, IO33-IO42, IO45-IO48.
- Avoid strapping/USB/JTAG/reserved pins until verified for this exact board [TENTATIVE].

## Missing evidence

- ESP32-S3-DEV-KIT-N32R16V-M board datasheet or schematic.
- Exact list of strapping/reserved pins for this board revision.
- Confirmation of any on-board peripherals that consume specific GPIOs.
- Confirmation of USB CDC / UART routing and which pins are safe for serial/UART use.

## Module coverage checklist (UI-listed modules)

| Module (UI) | Required interface/pins | S3 board provides? | Notes |
| --- | --- | --- | --- |
| PN532 NFC reader (SPI) | SPI bus + CS + optional IRQ/RST; UI uses SPI and safe pin lists in `data/app.js`. | Likely [TENTATIVE] | Needs verified SPI pins and safe CS/IRQ/RST allowlist. |
| microSD module | SPI bus + CS; UI uses `SD_CS_SAFE_PINS` in `data/app.js`. | Likely [TENTATIVE] | Needs verified SPI pins and CS pin. |
| DS3231 RTC | I2C SDA/SCL; time manager falls back to 21/22 in `src/storage/time_manager.cpp`. | Likely [TENTATIVE] | Needs verified I2C pins on the S3 header. |
| Motion sensor (GPIO) | Input GPIOs; UI uses `INPUT_GPIO_SAFE_PINS` in `data/app.js`. | Likely [TENTATIVE] | Needs verified input-capable GPIOs. |
| Motion sensor (LD2410B UART) | UART RX/TX pins; UI defaults RX=16, TX=17 in `data/app.js`. | Likely [TENTATIVE] | Needs UART-capable GPIOs and safe allowlist. |
| Door sensors | Input GPIOs; same allowlist as above. | Likely [TENTATIVE] | Needs verified input-capable GPIOs. |
| Enclosure/tamper sensor | Input GPIO; input-only pins allowed in UI. | Likely [TENTATIVE] | Needs verified input-capable GPIOs. |
| Outputs (horn/light) | Output-capable GPIOs; UI uses `OUTPUT_GPIO_SAFE_PINS` in `data/app.js`. | Likely [TENTATIVE] | Needs verified output-capable GPIOs. |
