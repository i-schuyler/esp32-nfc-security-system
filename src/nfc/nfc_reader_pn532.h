// src/nfc/nfc_reader_pn532.h
// Role: PN532 reader + minimal NDEF Type 2 write support (M6 slice 6).
#pragma once

#include <Arduino.h>

struct WssNfcTagInfo {
  uint8_t uid[10];
  uint8_t uid_len = 0;
  uint32_t capacity_bytes = 0;
};

struct WssNfcPn532Config {
  bool use_spi = false;
  int spi_cs_gpio = -1;
  int spi_irq_gpio = -1;
  int spi_rst_gpio = -1;
};

class WssNfcReaderPn532 {
 public:
  bool begin(const WssNfcPn532Config& cfg);
  bool poll(WssNfcTagInfo& out);
  bool write_ndef(const uint8_t* ndef, size_t len, uint32_t& bytes_written, String& err);
  bool ok() const { return _ok; }
  const String& last_error() const { return _last_error; }

 private:
  bool _ok = false;
  bool _use_spi = false;
  int _spi_cs_gpio = -1;
  int _spi_irq_gpio = -1;
  int _spi_rst_gpio = -1;
  uint32_t _last_poll_ms = 0;
  uint8_t _last_uid[10];
  uint8_t _last_uid_len = 0;
  String _last_error;

  bool read_capacity(uint32_t& out_capacity);
  bool write_pages(const uint8_t* data, size_t len, uint32_t capacity, String& err);
  void set_last_uid(const uint8_t* uid, uint8_t uid_len);
};
