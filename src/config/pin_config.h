// src/config/pin_config.h
// Role: Central compile-time pin configuration with safe defaults (no invented pin map).
//
// The canonical pin map is intentionally TBD in docs/Hardware_and_Wiring_v1_0.md.
// Therefore this file defaults pins to "unset" (-1). Firmware must not assume wiring.
//
// Example PlatformIO overrides (once wiring is confirmed):
//   build_flags =
//     -D WSS_PIN_I2C_SDA=21
//     -D WSS_PIN_I2C_SCL=22
//     -D WSS_PIN_SD_CS=5
//     -D WSS_PIN_SPI_SCK=18
//     -D WSS_PIN_SPI_MISO=19
//     -D WSS_PIN_SPI_MOSI=23

#pragma once

// I2C (DS3231 RTC, optional I2C peripherals)
#ifndef WSS_PIN_I2C_SDA
#define WSS_PIN_I2C_SDA (-1)
#endif
#ifndef WSS_PIN_I2C_SCL
#define WSS_PIN_I2C_SCL (-1)
#endif

// NFC (PN532) I2C control pins (optional; default unset)
#ifndef WSS_PIN_NFC_IRQ
#define WSS_PIN_NFC_IRQ (-1)
#endif
#ifndef WSS_PIN_NFC_RESET
#define WSS_PIN_NFC_RESET (-1)
#endif

// SPI (microSD)
// NOTE: CS is required to attempt SD initialization.
#ifndef WSS_PIN_SD_CS
#define WSS_PIN_SD_CS (-1)
#endif

// Optional SPI bus pin overrides. If left unset, Arduino SPI defaults are used.
#ifndef WSS_PIN_SPI_SCK
#define WSS_PIN_SPI_SCK (-1)
#endif
#ifndef WSS_PIN_SPI_MISO
#define WSS_PIN_SPI_MISO (-1)
#endif
#ifndef WSS_PIN_SPI_MOSI
#define WSS_PIN_SPI_MOSI (-1)
#endif

// Outputs (horn + light)
// The canonical V1 pin map is TBD, so these default to "unset".
// Firmware must treat unset pins as "output disabled" and surface that in /api/status.
#ifndef WSS_PIN_HORN_OUT
#define WSS_PIN_HORN_OUT (-1)
#endif
#ifndef WSS_PIN_LIGHT_OUT
#define WSS_PIN_LIGHT_OUT (-1)
#endif

// Sensors (M5)
// The canonical V1 pin map is TBD in docs/Hardware_and_Wiring_v1_0.md.
// These default to "unset". Firmware must treat unset pins as "not configured".
#ifndef WSS_PIN_MOTION_1
#define WSS_PIN_MOTION_1 (-1)
#endif
#ifndef WSS_PIN_MOTION_2
#define WSS_PIN_MOTION_2 (-1)
#endif

#ifndef WSS_PIN_DOOR_1
#define WSS_PIN_DOOR_1 (-1)
#endif
#ifndef WSS_PIN_DOOR_2
#define WSS_PIN_DOOR_2 (-1)
#endif

#ifndef WSS_PIN_ENCLOSURE_OPEN
#define WSS_PIN_ENCLOSURE_OPEN (-1)
#endif
