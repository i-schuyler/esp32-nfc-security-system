ARTIFACT — hardware_wiring.md (Hardware & Wiring v1.0)

# Hardware & Wiring Contract (V1)

This doc defines the V1 hardware assumptions and wiring contract.

**Photos provided show:**
- ESP32 DevKit v1 style board (ESP-WROOM-32 module)
- Type 2 NFC tag (wallet card style)

## Supported Hardware (V1)

- Supported baseline: **ESP32 DevKit V1 (ESP-WROOM-32)**. Wiring defaults are defined in `docs/Wiring_Instructions_DevKitV1_v1_0.md`.
- Planned / in-progress: **ESP32-S3-DEV-KIT-N32R16V-M** (Waveshare ESP32-S3-Dev-Kit-N32R16V). Investigation only; no canonical pin map or allowlist yet. See `docs/_investigation/esp32-s3-dev-kit-n32r16v-m_pin_roles_and_allowlist_notes.md`.

## 1) Required Modules (V1)

Power measurement (V1 decision):
- Battery voltage measurement via **ADC divider** (configurable/opt‑out in UI)

From accepted proposal:
- ESP32 DevKit (ESP-WROOM-32)
- PN532 NFC module (I2C or SPI)
- microSD module (SPI)
- DS3231 RTC (I2C)
- Motion sensor (LD2410 / LD2410B) (interface TBD: UART likely)
- Outputs: horn + light (via driver circuitry)
- Optional: door reed switch(es)
- Optional: enclosure tamper switch
- Optional: accelerometer (ADXL345, I2C)

## 2) Voltage Domains

- ESP32 IO: 3.3V
- Many PN532 boards accept 3.3V or 5V; must be verified for your exact module.
- SD module level shifting depends on module; safest is 3.3V-native SPI SD module.

**No assumptions:** final wiring depends on your exact PN532/SD breakout boards.

## 3) Connector Philosophy (Modularity)

- Use labeled pluggable connectors where possible.
- Standardize pin headers and JST connectors for field-replaceable modules.
- Document pin mapping on a printed label inside enclosure (recommended).

## 4) Pin Map (TBD)

The project will publish a canonical pin map once you confirm:
- PN532 bus choice (I2C vs SPI)
- SD module pin needs
- LD2410 interface pins
- Output driver pins
- Optional inputs (door/tamper/accel)

This is intentionally left **TBD** to avoid inventing pins.

## M7.3 Runtime Pin Configuration (Setup Wizard)

Planned (M7.3): This section defines the runtime pin-selection contract and the Setup Wizard UI/firmware handshake.

Devices covered in M7.3 pin selection:
- PN532 NFC (SPI) + optional IRQ/RST
- microSD (SPI)
- DS3231 RTC (I2C)
- LD2410B motion (UART)
- (Optional later) ADXL345 (I2C), reed/tamper inputs (GPIO)

UI/firmware contract:
- Setup Wizard must allow selecting module part number / interface mode and pins, and reflect it in runtime behavior.
- Must provide sane pin options (allowlist; exclude risky pins).
- No step blocked by missing hardware; missing hardware shows Unknown + guided checks.

Persistence:
- V1 persists config in NVS and must survive reboot + OTA.
- SD+NVS dual-save is deferred to V2.

Security:
- Never display or log secrets (Wi-Fi passwords, admin password, tokens, raw NFC UID).

Default recommended pin map:
- Recommended defaults are defined in `Wiring_Instructions_DevKitV1_v1_0.md`.

## Pin Allowlist Policy (DevKit V1)

The Setup Wizard enforces a conservative, board-specific allowlist for runtime pin selection:
- GPIO34-39 are input-only and are valid only for input roles (for example, NFC IRQ); they are invalid for TX/CS/RST/outputs.
- CS and RST must be output-capable pins.
- TX must be an output-capable pin.
- Optional pins set to "Not used" do not claim a GPIO.
- Duplicate assignments block completion only when the conflict is provable.

## 5) Output Driver Requirements

Horn and light likely exceed ESP32 GPIO current.

Contract:
- GPIO drives a transistor/MOSFET or relay module rated for load.
- Include flyback diode for inductive loads (relay/horn coil).
- Outputs default OFF on boot until firmware has initialized state.
- Output polarity must match the driver board:
  - `horn_active_low` / `light_active_low` (bool, default false).
  - `active_low=false` means GPIO HIGH = ON, GPIO LOW = OFF.
  - `active_low=true` means GPIO LOW = ON, GPIO HIGH = OFF.
  - Many relay boards are active-low; many low-side MOSFET drivers are active-high.
  - Bench test polarity before arming; inverted polarity can energize outputs unexpectedly.

## 6) Tamper Signals

V1 required (per proposal):
- NFC reader disconnect/cable cut detection (how detected is hardware-dependent)
Encouraged:
- enclosure open switch
- accelerometer “moved”

## 7) SD + RTC Placement

- SD and RTC must be physically protected and not easily removable without triggering tamper (recommended).
- System must remain functional without SD (fallback logs), but must visibly warn.
