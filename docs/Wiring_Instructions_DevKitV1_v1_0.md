Status: This wiring spec is the target for M7.3 (runtime pin selection + PN532 SPI + LD2410B UART).
Note: At the time of writing, the firmware PN532 driver is I2C-based; M7.3 will add SPI support to match this wiring.

# ESP32 NFC Security System — Wiring Instructions + Screw-Terminal Map (ESP32 DevKit V1)

> Target MCU: **ESP32 DevKit V1 (ESP-WROOM-32, CP2102)**  
> Source: your confirmed parts list + pin policy defaults (2026-02-02)  
> Design intent: **offline-first, predictable, fail-safe, minimal surprise**.

---

## 0) Parts covered in this document

### 0.1 Confirmed modules
- **PN532 NFC module** (SPI)
- **MicroSD module** (SPI) — HiLetgo 6-pin SPI module with level shifting
- **DS3231 RTC** (I2C)
- **ADXL345 accelerometer** (I2C)
- **LD2410B motion sensor** (UART)
- **Optional**: 1–2 door reed switches (digital inputs)
- **Optional**: enclosure tamper switch (digital input)

### 0.2 Not covered (because exact driver hardware not provided)
- Horn / strobe / steady light power switching hardware
- Battery / mains detection hardware
- Enclosure siren/strobe current requirements, fusing, etc.

I include **recommended patterns** for outputs later, but you’ll still need to choose a driver module (MOSFET/relay/transistor) sized for your loads.

---

## 1) Power and signal-level rules (read before wiring)

### 1.1 Ground is sacred
All modules must share **common GND** with the ESP32.

### 1.2 ESP32 logic level
ESP32 GPIO are **3.3V logic**. Avoid driving them with 5V signals.

If any module outputs 5V logic on its serial/data pins, use a **level shifter** or a safe divider.

### 1.3 Module power rails (practical guidance)
- ESP32 DevKit V1 provides:
  - **5V** on `VIN` / `5V` pin (from USB or external 5V)
  - **3V3** on `3V3` pin (from onboard regulator)
- **DS3231** modules usually accept 3.3V or 5V. Prefer **3.3V** if possible.
- **ADXL345** breakout boards vary: many are 3.3V; some have regulators. Prefer **3.3V** unless the board explicitly supports 5V.
- **PN532** breakout boards vary. Prefer powering at **3.3V** for best SPI signal compatibility unless the board explicitly supports 5V logic.
- **HiLetgo MicroSD** module with level shifting can often be powered at **5V**; however, SD cards are 3.3V devices and some level-shifter boards are noisy. If it has a 3.3V regulator onboard you can still power at 5V; if you can power at **3.3V**, that’s often cleaner.

### 1.4 I2C pull-ups
Many RTC/IMU breakouts include pull-ups on SDA/SCL. Two modules with pull-ups is usually fine, but if you see I2C instability, consider removing one set of pull-ups or ensuring total pull-up resistance isn’t too low (rule of thumb: total 2k–10k depending on bus length/capacitance).

---

## 2) Default pin map (ESP32 DevKit V1)

This is the **sane default** mapping intended to avoid common boot/flash pitfalls.

### 2.1 SPI bus (shared by PN532 + microSD)
- SCK  = **GPIO 18**
- MISO = **GPIO 19**
- MOSI = **GPIO 23**
- microSD CS = **GPIO 13**
- PN532 CS   = **GPIO 27**
- PN532 RST (optional) = **GPIO 33**
- PN532 IRQ (optional) = **GPIO 32**

### 2.2 I2C bus (shared by DS3231 + ADXL345)
- SDA = **GPIO 21**
- SCL = **GPIO 22**

### 2.3 UART (LD2410B)
- ESP32 RX2 = **GPIO 16**  ← connect to LD2410B **TX**
- ESP32 TX2 = **GPIO 17**  → connect to LD2410B **RX**

### 2.4 Digital inputs
- Reed switch 1 = **GPIO 25** (input pull-up)
- Reed switch 2 = **GPIO 26** (input pull-up)
- Optional tamper switch = **GPIO 34** (input-only)

> Note: GPIO34–GPIO39 are input-only, ideal for sensors/switches.

---

## 3) Screw-terminal map (recommended layout)

If you use a screw-terminal breakout/terminal strip, label terminals by **function**, not by raw GPIO number.
Below is a clean layout that makes wiring predictable and future-proof.

### 3.1 Power terminals
- **+5V**
- **+3V3**
- **GND** (at least 2–4 ground terminals; grounds get crowded)

### 3.2 I2C terminals (bus)
- **I2C_SDA (GPIO21)**
- **I2C_SCL (GPIO22)**
- Optional: **I2C_GND**, **I2C_3V3** (if you want a dedicated I2C 4-wire header)

### 3.3 SPI terminals (bus)
- **SPI_SCK (GPIO18)**
- **SPI_MISO (GPIO19)**
- **SPI_MOSI (GPIO23)**
- **SPI_GND**
- Optional: **SPI_3V3** (if you power PN532 from 3.3V)

### 3.4 SPI chip selects / control lines
- **SD_CS (GPIO13)**
- **NFC_CS (GPIO27)**
- **NFC_RST (GPIO33)** (optional)
- **NFC_IRQ (GPIO32)** (optional)

### 3.5 UART terminals (LD2410B)
- **UART2_RX (GPIO16)** (from sensor TX)
- **UART2_TX (GPIO17)** (to sensor RX)
- **UART_GND**
- **UART_5V** or **UART_3V3** (depending on LD2410B power requirements)

### 3.6 Digital inputs
- **REED1 (GPIO25)**
- **REED2 (GPIO26)**
- **TAMPER (GPIO34)**

### 3.7 Outputs (placeholders — depends on your driver hardware)
- **HORN_OUT** (GPIO TBD)
- **STROBE_OUT** (GPIO TBD)
- **LIGHT_OUT** (GPIO TBD)

> These should go to a **driver stage** (MOSFET/transistor/relay), not directly to a siren/strobe.

---

## 4) Wiring instructions (step-by-step)

### 4.1 Before you start
1) Power off everything.  
2) Decide your power distribution:
   - If powering from USB during bench test: use ESP32 USB as the 5V source.
   - If using an external 5V supply: feed ESP32 `VIN/5V` and share GND.

3) Keep SPI/I2C wires short, especially if mounted in a box. For a wall reader, plan for the PN532 antenna cable/placement carefully.

---

## 5) Connect the I2C devices (DS3231 + ADXL345)

### 5.1 DS3231 RTC → ESP32
- DS3231 **VCC** → **3V3** (preferred) or 5V (only if the module supports it)
- DS3231 **GND** → **GND**
- DS3231 **SDA** → **GPIO21**
- DS3231 **SCL** → **GPIO22**

### 5.2 ADXL345 → ESP32 (I2C)
- ADXL345 **VCC** → **3V3** (preferred)
- ADXL345 **GND** → **GND**
- ADXL345 **SDA** → **GPIO21**
- ADXL345 **SCL** → **GPIO22**
- Optional interrupt pins (if you use them later): route to spare inputs (e.g., GPIO35/36/39)

**Test tip:** Once wired, you should be able to scan I2C and see both devices.

---

## 6) Connect the SPI devices (microSD + PN532)

### 6.1 microSD module → ESP32 (SPI)
- microSD **VCC** → 5V *or* 3V3 (see module markings; choose the safer supported option)
- microSD **GND** → GND
- microSD **SCK/CLK** → GPIO18
- microSD **MISO/DO** → GPIO19
- microSD **MOSI/DI** → GPIO23
- microSD **CS** → GPIO13

### 6.2 PN532 module → ESP32 (SPI mode)
PN532 breakouts vary. Ensure it is set for **SPI** (some boards have jumpers/solder pads).

- PN532 **VCC** → 3V3 (preferred unless board supports 5V + 3.3V logic cleanly)
- PN532 **GND** → GND
- PN532 **SCK** → GPIO18
- PN532 **MISO** → GPIO19
- PN532 **MOSI** → GPIO23
- PN532 **SS/CS** → GPIO27
- PN532 **RST** (if exposed/used) → GPIO33
- PN532 **IRQ** (if exposed/used) → GPIO32

**Important:** SPI MISO lines share the bus. This is normal. Each device must tri-state MISO when not selected; reputable SD and PN532 boards do.

---

## 7) Connect the LD2410B (UART)

LD2410B modules often want **5V power**. The UART logic levels may be 3.3V or 5V depending on module.
**If you are not 100% sure the LD2410B TX is 3.3V, add a level shifter or a divider.**

- LD2410B **VCC** → 5V (most common) *(confirm your module)*
- LD2410B **GND** → GND
- LD2410B **TX** → ESP32 **GPIO16 (RX2)**
- LD2410B **RX** → ESP32 **GPIO17 (TX2)**

---

## 8) Reed switches (door sensors)

A reed switch is just a contact closure. Easiest wiring is **input pull-up**:
- One side of reed switch → GPIO input
- Other side → GND

### Reed 1
- Reed 1 → GPIO25
- Reed 1 other lead → GND

### Reed 2 (optional)
- Reed 2 → GPIO26
- Reed 2 other lead → GND

If the wire run is long (door frame), consider:
- Twisted pair
- Debounce in firmware
- Optional RC filter or shielded cable
- “Tamper aware” logging for line cut/short (future enhancement)

---

## 9) Optional tamper switch

Same pattern as reed:
- Tamper switch → GPIO34
- Other lead → GND

GPIO34 is input-only and great for this.

---

## 10) Output drivers (recommended patterns)

**Do not** connect sirens/strobes directly to ESP32 GPIO.

Pick one of these:
1) **Logic-level N-MOSFET module** (for DC loads)
   - ESP32 GPIO drives MOSFET gate (often through a resistor)
   - Load powered from external supply
   - Flyback diode required for inductive loads (relay, solenoid)

2) **Relay module** (if you need galvanic isolation or AC loads)
   - Beware relay coil current + noise; isolate and decouple

3) **Transistor + optocoupler** (if you need isolation)

When you choose driver modules and loads, I’ll produce the exact output wiring and fuse notes.

---

## 11) First bench wiring test sequence (minimal risk)

1) **ESP32 only**: power up, ensure serial works.
2) Add **I2C devices**: DS3231, ADXL345 → verify detection.
3) Add **microSD**: verify mount and log write.
4) Add **PN532**: verify reads.
5) Add **LD2410B**: verify UART comms.
6) Add **reed/tamper**: verify inputs change state.
7) Only after all above: wire **output drivers** (horn/strobe/light) and test in “test mode”.
