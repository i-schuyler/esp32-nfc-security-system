ESP32-S3 DEV-KIT (Row 1 = USB end) — Combined wiring (UART NFC + SD SPI + DS3231 I2C)

POWER (shared)
- GND  = Row 1 Right
- 3V3  = Row 21 Right

ELECHOUSE NFC V3 (UART)
- GND  -> Row 1 Right (GND)
- VCC  -> Row 21 Right (3V3)
- TXD  -> Row 20 Left (RXD)
- RXD  -> Row 21 Left (TXD)

DS3231 RTC (I2C)
- GND  -> Row 1 Right (GND)
- VCC  -> Row 21 Right (3V3)
- SCL  -> Row 8 Right (IO9)
- SDA  -> Row 11 Right (IO8)

MicroSD (SPI)
- GND  -> Row 1 Right (GND)
- VCC  -> Row 21 Right (3V3)
- CS   -> Row 4 Right (IO13)
- SCK  -> Row 5 Right (IO12)
- MISO -> Row 6 Right (IO11)
- MOSI -> Row 7 Right (IO10)