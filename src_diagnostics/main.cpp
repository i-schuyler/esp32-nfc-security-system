// src_diagnostics/main.cpp
// Board bring-up diagnostics firmware (non-customer product).

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

#ifndef WSS_DIAG_I2C_SDA
#define WSS_DIAG_I2C_SDA 21
#endif
#ifndef WSS_DIAG_I2C_SCL
#define WSS_DIAG_I2C_SCL 22
#endif
#ifndef WSS_DIAG_SPI_SCK
#define WSS_DIAG_SPI_SCK 18
#endif
#ifndef WSS_DIAG_SPI_MISO
#define WSS_DIAG_SPI_MISO 19
#endif
#ifndef WSS_DIAG_SPI_MOSI
#define WSS_DIAG_SPI_MOSI 23
#endif
#ifndef WSS_DIAG_SD_CS
#define WSS_DIAG_SD_CS 13
#endif
#ifndef WSS_DIAG_PN532_UART_RX
#define WSS_DIAG_PN532_UART_RX (-1)
#endif
#ifndef WSS_DIAG_PN532_UART_TX
#define WSS_DIAG_PN532_UART_TX (-1)
#endif
#ifndef WSS_DIAG_PN532_UART_BAUD
#define WSS_DIAG_PN532_UART_BAUD 115200
#endif

#ifndef WSS_DIAG_TARGET_ESP32S3
#define WSS_DIAG_TARGET_ESP32S3 0
#endif

static const uint32_t kPromptTimeoutMs = 15000;
static const uint32_t kPn532TimeoutMs = 1200;

static String read_line(uint32_t timeout_ms) {
  String line;
  uint32_t start_ms = millis();
  while ((uint32_t)(millis() - start_ms) < timeout_ms) {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        line.trim();
        return line;
      }
      if (line.length() < 96) line += c;
    }
    delay(10);
  }
  line.trim();
  return line;
}

static bool parse_int(const String& s, int& out) {
  if (s.length() == 0) return false;
  char* end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (end == s.c_str()) return false;
  out = (int)v;
  return true;
}

static int prompt_pin(const char* label, int def) {
  Serial.print(label);
  Serial.print(" [");
  if (def >= 0) Serial.print(def);
  else Serial.print("default");
  Serial.print("]: ");
  String line = read_line(kPromptTimeoutMs);
  if (line.length() == 0) return def;
  int v = def;
  if (parse_int(line, v)) return v;
  return def;
}

static bool prompt_sd_erase() {
  Serial.println("SD erase/format is DESTRUCTIVE and intended only for diagnostics.");
  Serial.println("Type ERASE to continue, or press Enter to skip (default NO)." );
  String line = read_line(kPromptTimeoutMs);
  line.trim();
  line.toUpperCase();
  return line == "ERASE";
}

static bool is_denied_pin(int pin) {
  if (pin < 0) return true;
  if (pin == 0 || pin == 1 || pin == 3) return true; // boot + UART0
  if (pin == 35 || pin == 36 || pin == 37) return true; // ESP32-S3 internal flash/PSRAM
#if !WSS_DIAG_TARGET_ESP32S3
  if (pin == 2 || pin == 4 || pin == 5 || pin == 12 || pin == 15) return true; // ESP32 strap pins
#endif
  return false;
}

static void factory_restore_best_effort() {
  Serial.println("[STEP 0] Factory restore (best-effort)");
  esp_err_t err = nvs_flash_erase();
  Serial.printf("- NVS erase: %s\n", (err == ESP_OK) ? "OK" : "FAIL");
  err = nvs_flash_init();
  Serial.printf("- NVS init: %s\n", (err == ESP_OK) ? "OK" : "FAIL");

  bool fs_ok = false;
  if (LittleFS.begin(false)) {
    fs_ok = LittleFS.format();
    LittleFS.end();
  } else if (LittleFS.begin(true)) {
    fs_ok = LittleFS.format();
    LittleFS.end();
  }
  if (fs_ok) {
    Serial.println("- LittleFS format: OK");
  } else {
    Serial.println("- LittleFS format: SKIP (not configured or mount failed)");
  }
}

static void test_psram() {
  Serial.println("[STEP 1] PSRAM test");
  if (!psramFound()) {
    Serial.println("- PSRAM: NOT FOUND");
    return;
  }
  Serial.println("- PSRAM: FOUND");
  const size_t sizes[] = {4096, 16384, 65536, 262144};
  for (size_t i = 0; i < (sizeof(sizes) / sizeof(sizes[0])); i++) {
    size_t sz = sizes[i];
    uint8_t* buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
      Serial.printf("- alloc %u bytes: FAIL\n", (unsigned)sz);
      continue;
    }
    memset(buf, 0xAA, sz);
    bool ok = true;
    for (size_t j = 0; j < sz; j++) {
      if (buf[j] != 0xAA) { ok = false; break; }
    }
    memset(buf, 0x55, sz);
    for (size_t j = 0; j < sz && ok; j++) {
      if (buf[j] != 0x55) { ok = false; break; }
    }
    heap_caps_free(buf);
    Serial.printf("- alloc %u bytes: %s\n", (unsigned)sz, ok ? "PASS" : "FAIL");
  }
}

static void test_partitions() {
  Serial.println("[STEP 1] Flash partition summary");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY,
                                                   ESP_PARTITION_SUBTYPE_ANY, nullptr);
  esp_partition_iterator_t it_head = it;
  while (it) {
    const esp_partition_t* p = esp_partition_get(it);
    uint8_t buf[16] = {0};
    esp_err_t err = esp_partition_read(p, 0, buf, sizeof(buf));
    uint32_t sum = 0;
    if (err == ESP_OK) {
      for (size_t i = 0; i < sizeof(buf); i++) sum += buf[i];
    }
    Serial.printf("- %s type=%u subtype=0x%02x addr=0x%06x size=%u read=%s sum=%u\n",
                  p->label, p->type, p->subtype, p->address, p->size,
                  (err == ESP_OK) ? "OK" : "FAIL", sum);
    it = esp_partition_next(it);
  }
  if (it_head) esp_partition_iterator_release(it_head);
}

static void test_i2c_scan() {
  Serial.println("[STEP 2] I2C scan");
  Serial.printf("- Using SDA=%d SCL=%d\n", WSS_DIAG_I2C_SDA, WSS_DIAG_I2C_SCL);
  Wire.begin(WSS_DIAG_I2C_SDA, WSS_DIAG_I2C_SCL);
  int found = 0;
  bool ds3231 = false;
  for (uint8_t addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  - I2C device: 0x%02X\n", addr);
      found++;
      if (addr == 0x68) ds3231 = true;
    }
  }
  Serial.printf("- I2C devices found: %d\n", found);
  Serial.printf("- DS3231 present: %s\n", ds3231 ? "YES" : "NO");
}

static bool sd_rw_test(SdFs& sd) {
  FsFile f = sd.open("/diag.tmp", O_RDWR | O_CREAT | O_TRUNC);
  if (!f) return false;
  const char* msg = "WSS-DIAG-SD";
  size_t len = strlen(msg);
  if (f.write((const uint8_t*)msg, len) != (int)len) { f.close(); return false; }
  f.flush();
  f.seekSet(0);
  char buf[32] = {0};
  int got = f.read((uint8_t*)buf, len);
  f.close();
  sd.remove("/diag.tmp");
  if (got != (int)len) return false;
  return strncmp(buf, msg, len) == 0;
}

static void test_sd() {
  Serial.println("[STEP 3] SD init + read/write test");
  Serial.printf("- SPI SCK=%d MISO=%d MOSI=%d CS=%d\n",
                WSS_DIAG_SPI_SCK, WSS_DIAG_SPI_MISO, WSS_DIAG_SPI_MOSI, WSS_DIAG_SD_CS);

  if (WSS_DIAG_SD_CS < 0) {
    Serial.println("- SD CS is unset; skipping SD test.");
    return;
  }

  SPI.begin(WSS_DIAG_SPI_SCK, WSS_DIAG_SPI_MISO, WSS_DIAG_SPI_MOSI, WSS_DIAG_SD_CS);
  SdFs sd;
  SdSpiConfig cfg(WSS_DIAG_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(20), &SPI);
  if (!sd.begin(cfg)) {
    Serial.println("- SD init: FAIL");
    return;
  }
  Serial.println("- SD init: OK");
  bool rw_ok = sd_rw_test(sd);
  Serial.printf("- SD read/write: %s\n", rw_ok ? "PASS" : "FAIL");

  if (prompt_sd_erase()) {
    Serial.println("- SD erase requested (destructive)." );
    auto* card = sd.card();
    if (card && card->sectorCount() > 0) {
      uint32_t last = card->sectorCount() - 1;
      bool ok = card->erase(0, last);
      Serial.printf("- SD erase blocks: %s\n", ok ? "OK" : "FAIL");
    } else {
      Serial.println("- SD erase blocks: FAIL (no card)");
    }
  } else {
    Serial.println("- SD erase skipped (default NO).");
  }
}

static void test_pin_probe() {
  Serial.println("[STEP 4] Pin probe toggle test");
#if WSS_DIAG_TARGET_ESP32S3
  const int pins[] = {8, 9, 10, 11, 12, 13};
#else
  const int pins[] = {13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
#endif
  for (size_t i = 0; i < (sizeof(pins) / sizeof(pins[0])); i++) {
    int pin = pins[i];
    if (is_denied_pin(pin)) {
      Serial.printf("- GPIO %d: SKIP (denylist)\n", pin);
      continue;
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(30);
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
    Serial.printf("- GPIO %d: TOGGLED\n", pin);
  }
}

static bool read_bytes(HardwareSerial& serial, uint8_t* buf, size_t len, uint32_t timeout_ms) {
  size_t got = 0;
  uint32_t start_ms = millis();
  while (got < len && (uint32_t)(millis() - start_ms) < timeout_ms) {
    if (serial.available() > 0) {
      buf[got++] = (uint8_t)serial.read();
    } else {
      delay(1);
    }
  }
  return got == len;
}

static void pn532_send_cmd(HardwareSerial& serial, const uint8_t* data, size_t len) {
  uint8_t preamble[] = {0x00, 0x00, 0xFF};
  serial.write(preamble, sizeof(preamble));
  uint8_t l = (uint8_t)len;
  uint8_t lcs = (uint8_t)(0x100 - l);
  serial.write(l);
  serial.write(lcs);
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
    serial.write(data[i]);
  }
  uint8_t dcs = (uint8_t)(0x100 - sum);
  serial.write(dcs);
  serial.write((uint8_t)0x00);
  serial.flush();
}

static bool pn532_get_firmware(HardwareSerial& serial, uint8_t* out, size_t out_len) {
  const uint8_t wake[] = {0x55, 0x55, 0x00, 0x00, 0x00};
  serial.write(wake, sizeof(wake));
  serial.flush();
  delay(50);

  const uint8_t cmd[] = {0xD4, 0x02}; // GetFirmwareVersion
  pn532_send_cmd(serial, cmd, sizeof(cmd));

  uint8_t ack[6] = {0};
  if (!read_bytes(serial, ack, sizeof(ack), kPn532TimeoutMs)) return false;
  const uint8_t expect_ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
  if (memcmp(ack, expect_ack, sizeof(ack)) != 0) return false;

  uint8_t header[5] = {0};
  if (!read_bytes(serial, header, sizeof(header), kPn532TimeoutMs)) return false;
  if (!(header[0] == 0x00 && header[1] == 0x00 && header[2] == 0xFF)) return false;
  uint8_t len = header[3];
  uint8_t lcs = header[4];
  if ((uint8_t)(len + lcs) != 0x00 || len < 2) return false;

  uint8_t data[32] = {0};
  if (len > sizeof(data)) return false;
  if (!read_bytes(serial, data, len, kPn532TimeoutMs)) return false;
  uint8_t dcs = 0;
  if (!read_bytes(serial, &dcs, 1, kPn532TimeoutMs)) return false;
  uint8_t post = 0;
  if (!read_bytes(serial, &post, 1, kPn532TimeoutMs)) return false;

  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++) sum += data[i];
  if ((uint8_t)(sum + dcs) != 0x00) return false;
  if (data[0] != 0xD5 || data[1] != 0x03) return false;
  if (out && out_len >= 4 && len >= 6) {
    out[0] = data[2];
    out[1] = data[3];
    out[2] = data[4];
    out[3] = data[5];
  }
  return true;
}

static void test_pn532_uart() {
  Serial.println("[STEP 5] PN532 UART (HSU) handshake");
  int rx = WSS_DIAG_PN532_UART_RX;
  int tx = WSS_DIAG_PN532_UART_TX;
  Serial.printf("- Default RX=%d TX=%d (override at prompt)\\n", rx, tx);
  rx = prompt_pin("Enter PN532 RX pin", rx);
  tx = prompt_pin("Enter PN532 TX pin", tx);
  Serial.printf("- Using RX=%d TX=%d\\n", rx, tx);

  HardwareSerial& pn_serial = Serial2;
  if (rx < 0 || tx < 0) {
    Serial.println("- Using Serial2 default pins (override by entering pins).");
    pn_serial.begin(WSS_DIAG_PN532_UART_BAUD);
  } else {
    if (is_denied_pin(rx) || is_denied_pin(tx)) {
      Serial.println("- PN532 UART test skipped: pin in denylist.");
      return;
    }
    pn_serial.begin(WSS_DIAG_PN532_UART_BAUD, SERIAL_8N1, rx, tx);
  }
  while (pn_serial.available()) pn_serial.read();
  delay(100);

  uint8_t fw[4] = {0};
  bool ok = pn532_get_firmware(pn_serial, fw, sizeof(fw));
  if (ok) {
    Serial.printf("- PN532 firmware: IC=0x%02X Ver=%u Rev=%u Support=0x%02X\n",
                  fw[0], fw[1], fw[2], fw[3]);
  } else {
    Serial.println("- PN532 firmware read: FAIL");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("DIAGNOSTIC FIRMWARE — NOT CUSTOMER PRODUCT");
#if WSS_DIAG_TARGET_ESP32S3
  Serial.println("Target: ESP32-S3 (diagnostic)");
#else
  Serial.println("Target: ESP32 (diagnostic)");
#endif

  factory_restore_best_effort();
  test_psram();
  test_partitions();
  test_i2c_scan();
  test_sd();
  test_pin_probe();
  test_pn532_uart();

  Serial.println("[DONE] Diagnostics complete.");
}

void loop() {
  delay(1000);
}
