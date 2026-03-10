// src/nfc/nfc_reader_pn532.cpp
// Role: PN532 reader + minimal NDEF Type 2 write support (M6 slice 6).

#include "nfc_reader_pn532.h"

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#include "../config/pin_config.h"

namespace {

static Adafruit_PN532* g_pn532 = nullptr;
static const uint32_t kPollIntervalMs = 120;
static HardwareSerial* g_uart = &Serial1;
static const uint32_t kPn532UartProbeDelayMs = 100;
static const uint32_t kPn532UartProbeTimeoutMs = 250;
static const uint32_t kPn532UartCommandTimeoutMs = 100;

static bool uart_read_bytes(HardwareSerial& serial, uint8_t* buf, size_t len, uint32_t timeout_ms) {
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

static uint32_t remaining_ms(uint32_t deadline_ms) {
  int32_t remaining = (int32_t)(deadline_ms - millis());
  return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void uart_drain(HardwareSerial& serial) {
  while (serial.available() > 0) serial.read();
}

static void pn532_uart_send_cmd(HardwareSerial& serial, const uint8_t* data, size_t len) {
  const uint8_t preamble[] = {0x00, 0x00, 0xFF};
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

static bool pn532_uart_exchange(HardwareSerial& serial, const uint8_t* cmd, size_t cmd_len,
                                uint8_t* payload, size_t& payload_len, uint32_t timeout_ms,
                                bool wake_first, bool* timed_out = nullptr) {
  if (timed_out) *timed_out = false;
  if (!cmd || cmd_len == 0 || !payload || payload_len == 0) return false;

  const uint8_t wake[] = {0x55, 0x55, 0x00, 0x00, 0x00};
  if (wake_first) {
    uart_drain(serial);
    serial.write(wake, sizeof(wake));
    serial.flush();
    delay(50);
  }

  const uint32_t deadline_ms = millis() + timeout_ms;
  pn532_uart_send_cmd(serial, cmd, cmd_len);

  uint8_t ack[6] = {0};
  uint32_t remaining = remaining_ms(deadline_ms);
  if (remaining == 0 || !uart_read_bytes(serial, ack, sizeof(ack), remaining)) {
    if (timed_out) *timed_out = true;
    return false;
  }
  const uint8_t expect_ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
  if (memcmp(ack, expect_ack, sizeof(ack)) != 0) return false;

  uint8_t header[5] = {0};
  remaining = remaining_ms(deadline_ms);
  if (remaining == 0 || !uart_read_bytes(serial, header, sizeof(header), remaining)) {
    if (timed_out) *timed_out = true;
    return false;
  }
  if (!(header[0] == 0x00 && header[1] == 0x00 && header[2] == 0xFF)) return false;
  uint8_t frame_len = header[3];
  if ((uint8_t)(frame_len + header[4]) != 0x00 || frame_len < 2) return false;
  if (frame_len > payload_len) return false;

  remaining = remaining_ms(deadline_ms);
  if (remaining == 0 || !uart_read_bytes(serial, payload, frame_len, remaining)) {
    if (timed_out) *timed_out = true;
    return false;
  }

  uint8_t dcs = 0;
  remaining = remaining_ms(deadline_ms);
  if (remaining == 0 || !uart_read_bytes(serial, &dcs, 1, remaining)) {
    if (timed_out) *timed_out = true;
    return false;
  }

  uint8_t postamble = 0;
  remaining = remaining_ms(deadline_ms);
  if (remaining == 0 || !uart_read_bytes(serial, &postamble, 1, remaining)) {
    if (timed_out) *timed_out = true;
    return false;
  }

  uint8_t sum = 0;
  for (uint8_t i = 0; i < frame_len; i++) sum += payload[i];
  if ((uint8_t)(sum + dcs) != 0x00) return false;
  if (postamble != 0x00) return false;

  payload_len = frame_len;
  return true;
}

static bool pn532_uart_get_firmware(HardwareSerial& serial, uint8_t* out, size_t out_len,
                                    uint32_t timeout_ms, bool* timed_out) {
  const uint8_t cmd[] = {0xD4, 0x02};
  uint8_t payload[16] = {0};
  size_t payload_len = sizeof(payload);
  if (!pn532_uart_exchange(serial, cmd, sizeof(cmd), payload, payload_len, timeout_ms, true, timed_out)) {
    return false;
  }
  if (payload_len < 6) return false;
  if (payload[0] != 0xD5 || payload[1] != 0x03) return false;
  if (out && out_len >= 4) {
    out[0] = payload[2];
    out[1] = payload[3];
    out[2] = payload[4];
    out[3] = payload[5];
  }
  return true;
}

static bool pn532_uart_sam_config(HardwareSerial& serial, uint32_t timeout_ms) {
  const uint8_t cmd[] = {0xD4, 0x14, 0x01, 0x14, 0x01};
  uint8_t payload[8] = {0};
  size_t payload_len = sizeof(payload);
  if (!pn532_uart_exchange(serial, cmd, sizeof(cmd), payload, payload_len, timeout_ms, false)) {
    return false;
  }
  return (payload_len >= 3 && payload[0] == 0xD5 && payload[1] == 0x15 && payload[2] == 0x00);
}

static bool pn532_uart_read_passive_target(HardwareSerial& serial, uint8_t* uid,
                                           uint8_t& uid_len, uint32_t timeout_ms) {
  const uint8_t cmd[] = {0xD4, PN532_COMMAND_INLISTPASSIVETARGET, 0x01, PN532_MIFARE_ISO14443A};
  uint8_t payload[32] = {0};
  size_t payload_len = sizeof(payload);
  if (!pn532_uart_exchange(serial, cmd, sizeof(cmd), payload, payload_len, timeout_ms, false)) {
    return false;
  }
  if (payload_len < 8) return false;
  if (payload[0] != 0xD5 || payload[1] != 0x4B) return false;
  if (payload[2] != 0x01) return false;
  uid_len = payload[7];
  if (uid_len == 0 || payload_len < (size_t)(8 + uid_len)) return false;
  memcpy(uid, payload + 8, uid_len);
  return true;
}

static bool pn532_uart_read_page(HardwareSerial& serial, uint8_t page, uint8_t* out_page) {
  const uint8_t cmd[] = {0xD4, PN532_COMMAND_INDATAEXCHANGE, 0x01, MIFARE_CMD_READ, page};
  uint8_t payload[24] = {0};
  size_t payload_len = sizeof(payload);
  if (!pn532_uart_exchange(serial, cmd, sizeof(cmd), payload, payload_len, kPn532UartCommandTimeoutMs, false)) {
    return false;
  }
  if (payload_len < 7) return false;
  if (payload[0] != 0xD5 || payload[1] != 0x41 || payload[2] != 0x00) return false;
  memcpy(out_page, payload + 3, 4);
  return true;
}

static bool pn532_uart_write_page(HardwareSerial& serial, uint8_t page, const uint8_t* data) {
  uint8_t cmd[] = {0xD4, PN532_COMMAND_INDATAEXCHANGE, 0x01, MIFARE_ULTRALIGHT_CMD_WRITE, page,
                   0x00, 0x00, 0x00, 0x00};
  memcpy(cmd + 5, data, 4);
  uint8_t payload[8] = {0};
  size_t payload_len = sizeof(payload);
  if (!pn532_uart_exchange(serial, cmd, sizeof(cmd), payload, payload_len, kPn532UartCommandTimeoutMs, false)) {
    return false;
  }
  delay(10);
  return (payload_len >= 3 && payload[0] == 0xD5 && payload[1] == 0x41 && payload[2] == 0x00);
}

static bool i2c_pins_configured() {
  return (WSS_PIN_I2C_SDA >= 0 && WSS_PIN_I2C_SCL >= 0 && WSS_PIN_NFC_IRQ >= 0 && WSS_PIN_NFC_RESET >= 0);
}

static void pulse_reset(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(10);
  digitalWrite(pin, HIGH);
  delay(10);
}

} // namespace

bool WssNfcReaderPn532::begin(const WssNfcPn532Config& cfg) {
  _ok = false;
  _last_error = "";
  _last_uid_len = 0;

  if (cfg.use_uart) {
    if (cfg.uart_rx_gpio < 0 || cfg.uart_tx_gpio < 0) {
      _last_error = "uart_pins_unset";
      return false;
    }
    if (_use_spi || !_use_uart || _uart_rx_gpio != cfg.uart_rx_gpio || _uart_tx_gpio != cfg.uart_tx_gpio) {
      if (g_pn532) {
        delete g_pn532;
        g_pn532 = nullptr;
      }
    }
    _use_spi = false;
    _use_uart = true;
    _uart_rx_gpio = cfg.uart_rx_gpio;
    _uart_tx_gpio = cfg.uart_tx_gpio;
    g_uart->end();
    pinMode(_uart_rx_gpio, INPUT_PULLUP);
    g_uart->begin(115200, SERIAL_8N1, _uart_rx_gpio, _uart_tx_gpio);
    uart_drain(*g_uart);
    delay(kPn532UartProbeDelayMs);

    uint8_t fw[4] = {0};
    bool timed_out = false;
    if (!pn532_uart_get_firmware(*g_uart, fw, sizeof(fw), kPn532UartProbeTimeoutMs, &timed_out)) {
      g_uart->end();
      _last_error = timed_out ? "pn532_not_found" : "pn532_probe_failed";
      return false;
    }
    if (!pn532_uart_sam_config(*g_uart, kPn532UartCommandTimeoutMs)) {
      g_uart->end();
      _last_error = "samconfig_failed";
      return false;
    }
    _ok = true;
    return true;
  } else if (cfg.use_spi) {
    if (cfg.spi_cs_gpio < 0) {
      _last_error = "spi_cs_unset";
      return false;
    }
    if (!_use_spi || _use_uart || _spi_cs_gpio != cfg.spi_cs_gpio) {
      if (g_pn532) {
        delete g_pn532;
        g_pn532 = nullptr;
      }
    }
    _use_spi = true;
    _use_uart = false;
    _spi_cs_gpio = cfg.spi_cs_gpio;
    _spi_irq_gpio = cfg.spi_irq_gpio;
    _spi_rst_gpio = cfg.spi_rst_gpio;
    SPI.begin(18, 19, 23);
    if (_spi_rst_gpio >= 0) {
      pulse_reset(_spi_rst_gpio);
    }
    if (!g_pn532) {
      g_pn532 = new Adafruit_PN532((uint8_t)_spi_cs_gpio);
    }
  } else {
    if (!i2c_pins_configured()) {
      _last_error = "pins_unset";
      return false;
    }
    if (_use_spi || _use_uart) {
      if (g_pn532) {
        delete g_pn532;
        g_pn532 = nullptr;
      }
    }
    _use_spi = false;
    _use_uart = false;
    Wire.begin(WSS_PIN_I2C_SDA, WSS_PIN_I2C_SCL);
    if (!g_pn532) {
      g_pn532 = new Adafruit_PN532(WSS_PIN_NFC_IRQ, WSS_PIN_NFC_RESET, &Wire);
    }
  }

  g_pn532->begin();
  uint32_t ver = g_pn532->getFirmwareVersion();
  if (!ver) {
    _last_error = "pn532_not_found";
    return false;
  }

  g_pn532->SAMConfig();
  _ok = true;
  return true;
}

bool WssNfcReaderPn532::poll(WssNfcTagInfo& out) {
  out.uid_len = 0;
  out.capacity_bytes = 0;
  if (!_ok) return false;

  uint32_t now_ms = millis();
  if ((uint32_t)(now_ms - _last_poll_ms) < kPollIntervalMs) return false;
  _last_poll_ms = now_ms;

  if (_use_uart) {
    uint8_t uid[10];
    uint8_t uid_len = 0;
    if (!pn532_uart_read_passive_target(*g_uart, uid, uid_len, 10)) return false;
    if (uid_len == 0 || uid_len > sizeof(uid)) return false;
    out.uid_len = uid_len;
    memcpy(out.uid, uid, uid_len);
    set_last_uid(uid, uid_len);
    return read_capacity(out.capacity_bytes);
  }
  if (!g_pn532) return false;

  uint8_t uid[10];
  uint8_t uid_len = 0;
  bool ok = g_pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uid_len, 10);
  if (!ok || uid_len == 0 || uid_len > sizeof(uid)) return false;

  out.uid_len = uid_len;
  memcpy(out.uid, uid, uid_len);
  set_last_uid(uid, uid_len);
  return read_capacity(out.capacity_bytes);
}

bool WssNfcReaderPn532::read_capacity(uint32_t& out_capacity) {
  out_capacity = 0;
  if (!_ok) return false;
  uint8_t page[4];
  bool read_ok = false;
  if (_use_uart) {
    read_ok = pn532_uart_read_page(*g_uart, 3, page);
  } else if (g_pn532) {
    read_ok = g_pn532->ntag2xx_ReadPage(3, page);
  }
  if (!read_ok) {
    _last_error = "cc_read_failed";
    return false;
  }
  if (page[0] != 0xE1) {
    _last_error = "cc_invalid";
    return false;
  }
  uint8_t size = page[2];
  out_capacity = (uint32_t)size * 8U;
  if (out_capacity == 0) {
    _last_error = "capacity_zero";
    return false;
  }
  return true;
}

bool WssNfcReaderPn532::write_pages(const uint8_t* data, size_t len, uint32_t capacity, String& err) {
  if (!_ok) return false;
  if (!data || len == 0) return false;
  if (len > capacity) {
    err = "payload_too_large";
    return false;
  }

  size_t padded = (len + 3) & ~((size_t)3);
  if (padded > capacity) {
    err = "payload_over_capacity";
    return false;
  }

  uint8_t page[4];
  size_t offset = 0;
  uint8_t page_idx = 4; // data starts at page 4

  while (offset < padded) {
    for (size_t i = 0; i < 4; i++) {
      size_t idx = offset + i;
      page[i] = (idx < len) ? data[idx] : 0x00;
    }
    bool write_ok = false;
    if (_use_uart) {
      write_ok = pn532_uart_write_page(*g_uart, page_idx, page);
    } else if (g_pn532) {
      write_ok = g_pn532->ntag2xx_WritePage(page_idx, page);
    }
    if (!write_ok) {
      err = "page_write_failed";
      return false;
    }
    offset += 4;
    page_idx++;
  }
  return true;
}

bool WssNfcReaderPn532::write_ndef(const uint8_t* ndef, size_t len, uint32_t& bytes_written, String& err) {
  bytes_written = 0;
  err = "";
  uint32_t capacity = 0;
  if (!read_capacity(capacity)) {
    err = _last_error.length() ? _last_error : "capacity_unknown";
    return false;
  }
  if (!write_pages(ndef, len, capacity, err)) {
    return false;
  }
  bytes_written = (uint32_t)len;
  return true;
}

void WssNfcReaderPn532::set_last_uid(const uint8_t* uid, uint8_t uid_len) {
  _last_uid_len = 0;
  if (!uid || uid_len == 0 || uid_len > sizeof(_last_uid)) return;
  memcpy(_last_uid, uid, uid_len);
  _last_uid_len = uid_len;
}
