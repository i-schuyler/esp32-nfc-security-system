# ESP32-S3-DEV-KIT-N32R16V-M Support Investigation Report

Scope: Slice 1 only (investigation + docs-only).

## Current repo facts (with paths)
### Build system / targets
- `platformio.ini` uses PlatformIO with Arduino framework; default env `esp32dev`, board `esp32dev`, `board_build.partitions = partitions.csv`, `board_build.filesystem = littlefs`, feature flags `WSS_FEATURE_*`.
- `partitions.csv` is a 4MB layout (app0/app1 + spiffs).
- No `sdkconfig` or `sdkconfig.defaults` files are present in the repo (searched via `rg --files -g "sdkconfig*"`).

### Pin mapping and board selection
- Compile-time pin defaults are defined in `src/config/pin_config.h` and all default to `-1` with a note that the canonical pin map is TBD in `docs/Hardware_and_Wiring_v1_0.md`.
- Runtime config defaults (pins and interfaces) are set in `src/config/config_store.cpp` (e.g., `nfc_interface=spi`, `nfc_spi_*` defaults, `motion_ld2410b_*` defaults).
- Pin allowlists for wizard validation are hardcoded for DevKit V1 in `src/web_server.cpp` (`output_pin_allowed`, `input_pin_allowed`).
- SD SPI bus pins are fixed to `18/19/23` in `src/storage/storage_manager.cpp` (`kSdSpiSck`, `kSdSpiMiso`, `kSdSpiMosi`); SD CS comes from config (`sd_cs_gpio`).
- PN532 SPI bus pins are fixed to `18/19/23` in `src/nfc/nfc_reader_pn532.cpp` (`SPI.begin(18, 19, 23)`); SPI CS/RST/IRQ are from config.
- RTC I2C pins are from `WSS_PIN_I2C_*` but fall back to `21/22` if unset in `src/storage/time_manager.cpp`.
- Canonical hardware docs cover ESP32 DevKit V1 only: `docs/Hardware_and_Wiring_v1_0.md` and `docs/Wiring_Instructions_DevKitV1_v1_0.md`.

### Docs that define pin policy
- `docs/Configuration_Registry_v1_0.md` defines DevKit V1 pin allowlist and pin-related config keys.
- `docs/Web_UI_Spec_v1_0.md` specifies M7.3 pin selection flow and mentions SPI/I2C/UART pin choices.

## Gaps / unknowns for ESP32-S3-DEV-KIT-N32R16V-M
- Canonical docs do not define an ESP32-S3 target, pin map, or pin allowlist; `docs/Hardware_and_Wiring_v1_0.md` explicitly says the pin map is TBD and only references DevKit V1.
- No PlatformIO env or board id for ESP32-S3 exists in `platformio.ini`.
- Code assumes DevKit V1 SPI bus pins (`18/19/23`) and DevKit V1 pin allowlists; S3-safe GPIOs and strapping pins are not defined in the repo.
- Partition table is 4MB (`partitions.csv`); N32R16V-M implies larger flash, but no larger layout is provided.

## Minimal strategies (least drift)
### Option A: New PlatformIO env only
Add a new env in `platformio.ini` for the S3 dev kit (board id TBD) and set `board_build.partitions` + `board_build.filesystem`. Use build flags to set `WSS_PIN_*` defaults.
- Pros: minimal diff; no code refactors.
- Cons: hardcoded SPI pins and DevKit V1 allowlists remain; S3 pins may still be unsafe or incorrect without code changes.

### Option B: Board-specific pin/allowlist layer (within existing modules)
Introduce a build flag (e.g., `WSS_TARGET_ESP32S3`) and update existing code paths to use `WSS_PIN_SPI_*`/`WSS_PIN_I2C_*` and board-specific allowlists instead of fixed numbers.
- Pros: avoids parallel stacks; pin policy centralized; safer for multiple boards.
- Cons: requires code changes in multiple modules; needs canonical S3 pin policy docs first.

## S3-specific concerns to verify (repo does not define these)
- PlatformIO board id and required PSRAM build flags for N32R16V-M.
- Flash size and whether to keep `partitions.csv` (4MB) or add a larger partition layout.
- USB CDC vs UART0 for serial logs (and any required build flags).
- Safe GPIO allowlist and strapping pin restrictions for this S3 dev kit.
- Actual SPI/I2C/UART pin assignments for PN532, SD, DS3231, and LD2410B.

## Recommended next slices
1) Docs-only anchor slice: add S3 wiring/pin allowlist docs and update `docs/Hardware_and_Wiring_v1_0.md` + `docs/Configuration_Registry_v1_0.md`.
2) Implementation slice: add S3 PlatformIO env (and optional partition file) with `WSS_PIN_*` defaults.
3) Implementation slice: replace fixed SPI bus pins with `WSS_PIN_SPI_*` overrides and update wizard allowlists in `src/web_server.cpp`.
4) Optional slice: update `docs/Web_UI_Spec_v1_0.md` to include S3-specific pin selection constraints.

## Minimal data needed from the user/hardware
- Exact PlatformIO board id (or board JSON) for ESP32-S3-DEV-KIT-N32R16V-M.
- Confirm flash size and PSRAM size.
- Desired SPI/I2C/UART pin assignments for PN532, SD, DS3231, and LD2410B.
- Safe GPIO allowlist for outputs/inputs on the S3 dev kit (including strapping pins).
- Whether USB CDC should be enabled for serial logs on this board.

## Stop conditions encountered
- Canonical docs only define ESP32 DevKit V1 and state the pin map is TBD; no S3 target/pin allowlist exists yet.
