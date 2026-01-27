// src/wifi/wifi_manager.cpp
// Role: Wi-Fi mode manager (STA attempt + AP fallback) driven by ConfigStore.

#include "wifi_manager.h"

#include <WiFi.h>
#include <ArduinoJson.h>

#include "../config/config_store.h"
#include "../logging/event_logger.h"

static void log_wifi_mode(WssEventLogger& log, const char* mode, const char* reason, const String& ssid, const String& ip) {
  StaticJsonDocument<256> extra;
  extra["mode"] = mode;
  extra["reason"] = reason;
  if (ssid.length()) extra["ssid"] = ssid; // SSID is permitted in logs.
  if (ip.length()) extra["ip"] = ip;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  log.log_info("wifi", "wifi_mode_change", String("wifi ") + mode, &o);
}

static void start_ap(const WssConfigStore& cfg, WssEventLogger& log) {
  String ssid = cfg.doc()["wifi_ap_ssid"] | String("Workshop Security System");
  String pass = cfg.doc()["wifi_ap_password"] | String("");
  if (pass.length() < 8) pass = "ChangeMe-XXXX"; // should not happen after ensure_runtime_defaults

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), pass.c_str());

  String ip = WiFi.softAPIP().toString();
  log_wifi_mode(log, "AP", "fallback_or_config", ssid, ip);
}

static bool try_sta(const WssConfigStore& cfg, WssEventLogger& log) {
  bool sta_en = cfg.doc()["wifi_sta_enabled"] | false;
  if (!sta_en) return false;

  String ssid = cfg.doc()["wifi_sta_ssid"] | String("");
  String pass = cfg.doc()["wifi_sta_password"] | String("");
  int timeout_s = cfg.doc()["wifi_sta_connect_timeout_s"] | 20;

  if (ssid.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  uint32_t deadline = start + (uint32_t)timeout_s * 1000UL;
  while (millis() < deadline) {
    if (WiFi.status() == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      log_wifi_mode(log, "STA", "sta_join_ok", ssid, ip);
      return true;
    }
    delay(100);
  }

  log_wifi_mode(log, "AP", "sta_join_failed", ssid, "");
  WiFi.disconnect(true, true);
  return false;
}

bool wss_wifi_begin(const WssConfigStore& cfg, const String& device_suffix, WssEventLogger& log) {
  (void)device_suffix;
  // Prefer STA if configured; otherwise AP.
  if (try_sta(cfg, log)) return true;
  start_ap(cfg, log);
  return true;
}

WssWifiStatus wss_wifi_status() {
  WssWifiStatus s;
  wifi_mode_t m = WiFi.getMode();
  if (m == WIFI_AP) {
    s.mode = "AP";
    s.ssid = WiFi.softAPSSID();
    s.ip = WiFi.softAPIP().toString();
    s.rssi = 0;
  } else if (m == WIFI_STA) {
    s.mode = "STA";
    s.ssid = WiFi.SSID();
    s.ip = WiFi.localIP().toString();
    s.rssi = WiFi.RSSI();
  } else {
    s.mode = "OTHER";
    s.ssid = "";
    s.ip = "";
    s.rssi = 0;
  }
  return s;
}
