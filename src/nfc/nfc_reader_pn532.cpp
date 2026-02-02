// src/nfc/nfc_reader_pn532.cpp
// Role: PN532 reader + minimal NDEF Type 2 write support (M6 slice 6).

#include "nfc_reader_pn532.h"

#include <Wire.h>
#include <Adafruit_PN532.h>

#include "../config/pin_config.h"

namespace {

static Adafruit_PN532* g_pn532 = nullptr;
static const uint32_t kPollIntervalMs = 120;

static bool pins_configured() {
  return (WSS_PIN_I2C_SDA >= 0 && WSS_PIN_I2C_SCL >= 0 && WSS_PIN_NFC_IRQ >= 0 && WSS_PIN_NFC_RESET >= 0);
}

} // namespace

bool WssNfcReaderPn532::begin() {
  _ok = false;
  _last_error = "";
  _last_uid_len = 0;

  if (!pins_configured()) {
    _last_error = "pins_unset";
    return false;
  }

  Wire.begin(WSS_PIN_I2C_SDA, WSS_PIN_I2C_SCL);
  if (!g_pn532) {
    g_pn532 = new Adafruit_PN532(WSS_PIN_NFC_IRQ, WSS_PIN_NFC_RESET, &Wire);
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
  if (!_ok || !g_pn532) return false;

  uint32_t now_ms = millis();
  if ((uint32_t)(now_ms - _last_poll_ms) < kPollIntervalMs) return false;
  _last_poll_ms = now_ms;

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
  if (!_ok || !g_pn532) return false;
  uint8_t page[4];
  if (!g_pn532->ntag2xx_ReadPage(3, page)) {
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
  if (!_ok || !g_pn532) return false;
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
    if (!g_pn532->ntag2xx_WritePage(page_idx, page)) {
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
