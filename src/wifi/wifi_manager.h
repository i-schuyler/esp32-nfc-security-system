// src/wifi/wifi_manager.h
// Role: Wi-Fi mode manager (STA attempt + AP fallback) driven by ConfigStore.
#pragma once

#include <Arduino.h>

class WssConfigStore;
class WssEventLogger;

struct WssWifiStatus {
  String mode;   // "AP" | "STA" | "OFF" | "UNKNOWN"
  String ssid;   // active SSID (if any)
  String ip;     // IP string
  int32_t rssi;  // STA RSSI if connected
};

bool wss_wifi_begin(const WssConfigStore& cfg, const String& device_suffix, WssEventLogger& log);
WssWifiStatus wss_wifi_status();
