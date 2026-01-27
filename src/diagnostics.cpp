// src/diagnostics.cpp
#include "diagnostics.h"
#include <WiFi.h>

static String resetReasonToString(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

static String macSuffixA1B2() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[5];
  // last 2 bytes -> 4 hex chars
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
}

WssBootInfo wss_get_boot_info() {
  WssBootInfo b;
  b.reset_reason = resetReasonToString(esp_reset_reason());
  b.chip_id_suffix = macSuffixA1B2();
  return b;
}
