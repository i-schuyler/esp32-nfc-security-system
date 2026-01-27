ARTIFACT — hardware_wiring.md (Hardware & Wiring v1.0)

# Hardware & Wiring Contract (V1)

This doc defines the V1 hardware assumptions and wiring contract.

**Photos provided show:**
- ESP32 DevKit v1 style board (ESP-WROOM-32 module)
- Type 2 NFC tag (wallet card style)

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

## 5) Output Driver Requirements

Horn and light likely exceed ESP32 GPIO current.

Contract:
- GPIO drives a transistor/MOSFET or relay module rated for load.
- Include flyback diode for inductive loads (relay/horn coil).
- Outputs default OFF on boot until firmware has initialized state.

## 6) Tamper Signals

V1 required (per proposal):
- NFC reader disconnect/cable cut detection (how detected is hardware-dependent)
Encouraged:
- enclosure open switch
- accelerometer “moved”

## 7) SD + RTC Placement

- SD and RTC must be physically protected and not easily removable without triggering tamper (recommended).
- System must remain functional without SD (fallback logs), but must visibly warn.
