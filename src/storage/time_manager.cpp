// src/storage/time_manager.cpp
// Role: RTC-backed time source (DS3231) + time validity reporting for M2.

#include "time_manager.h"

#include <time.h>

#include "../config/pin_config.h"
#include "../logging/event_logger.h"

#if WSS_FEATURE_RTC
#include <Wire.h>
#include <RTClib.h>
#endif

static WssTimeStatus g_time_status;
static uint32_t g_last_poll_ms = 0;
static WssEventLogger* g_log = nullptr;

#if WSS_FEATURE_RTC
static RTC_DS3231 g_rtc;

static bool i2c_probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

static void set_system_time_utc(uint32_t epoch_s) {
  struct timeval tv;
  tv.tv_sec = (time_t)epoch_s;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}
#endif

static bool system_time_valid() {
  time_t now = time(nullptr);
  return (now > 1700000000); // ~2023-11-14
}

String wss_time_now_iso8601_utc(bool& time_valid) {
  time_t now = time(nullptr);
  time_valid = system_time_valid();
  struct tm tm_utc;
  gmtime_r(&now, &tm_utc);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return String(buf);
}

static void emit_time_status_log(const char* event_type, const char* msg) {
  if (!g_log) return;
  StaticJsonDocument<256> extra;
  extra["status"] = g_time_status.status;
  extra["rtc_present"] = g_time_status.rtc_present;
  extra["time_valid"] = g_time_status.time_valid;
  extra["pinmap_configured"] = g_time_status.pinmap_configured;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_info("time", event_type, msg, &o);
}

void wss_time_begin(WssEventLogger* log) {
  g_log = log;
  g_time_status = WssTimeStatus{};

#if !WSS_FEATURE_RTC
  g_time_status.feature_enabled = false;
  g_time_status.status = "DISABLED";
  bool tv = false;
  g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
  g_time_status.time_valid = tv;
  return;
#else
  g_time_status.feature_enabled = true;

  // Pin map must be explicitly configured to avoid inventing wiring.
  if (WSS_PIN_I2C_SDA < 0 || WSS_PIN_I2C_SCL < 0) {
    g_time_status.pinmap_configured = false;
    g_time_status.status = "DISABLED";
    bool tv = false;
    g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
    g_time_status.time_valid = tv;
    if (g_log) g_log->log_warn("time", "rtc_disabled", "RTC disabled: pin map not configured");
    return;
  }

  g_time_status.pinmap_configured = true;
  Wire.begin(WSS_PIN_I2C_SDA, WSS_PIN_I2C_SCL);

  if (!i2c_probe(0x68) || !g_rtc.begin()) {
    g_time_status.rtc_present = false;
    g_time_status.status = "MISSING";
    bool tv = false;
    g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
    g_time_status.time_valid = tv;
    if (g_log) g_log->log_warn("time", "time_status", "RTC missing (DS3231 not detected)");
    return;
  }

  g_time_status.rtc_present = true;

  if (g_rtc.lostPower()) {
    g_time_status.time_valid = false;
    g_time_status.status = "TIME_INVALID";
    bool tv = false;
    g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
    if (g_log) g_log->log_warn("time", "time_status", "RTC present but time invalid (lostPower)");
    return;
  }

  // RTC is present and claims valid time: set system clock from RTC.
  DateTime now = g_rtc.now();
  uint32_t epoch = (uint32_t)now.unixtime();
  set_system_time_utc(epoch);

  bool tv = false;
  g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
  g_time_status.time_valid = tv;
  g_time_status.status = tv ? "OK" : "TIME_INVALID";

  emit_time_status_log("time_status", tv ? "RTC OK" : "RTC present but system time invalid");
#endif
}

void wss_time_loop() {
#if !WSS_FEATURE_RTC
  bool tv = false;
  g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
  g_time_status.time_valid = tv;
  return;
#else
  const uint32_t now_ms = millis();
  if ((uint32_t)(now_ms - g_last_poll_ms) < 2000) return;
  g_last_poll_ms = now_ms;

  bool tv = false;
  g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
  g_time_status.time_valid = tv;

  if (!g_time_status.feature_enabled) return;
  if (!g_time_status.pinmap_configured) return;

  // Probe RTC presence periodically to detect disconnects.
  bool present_now = i2c_probe(0x68);
  if (present_now != g_time_status.rtc_present) {
    g_time_status.rtc_present = present_now;
    g_time_status.status = present_now ? (g_time_status.time_valid ? "OK" : "TIME_INVALID") : "MISSING";
    if (g_log) {
      if (present_now) {
        g_log->log_warn("time", "time_status", "RTC detected (was missing)");
      } else {
        g_log->log_warn("time", "time_status", "RTC missing (was present)");
      }
    }
  }

  // If present, keep TIME_INVALID if lostPower is reported.
  if (g_time_status.rtc_present) {
    if (g_rtc.lostPower()) {
      if (g_time_status.status != "TIME_INVALID") {
        g_time_status.status = "TIME_INVALID";
        if (g_log) g_log->log_warn("time", "time_status", "RTC time invalid (lostPower)");
      }
    } else {
      g_time_status.status = g_time_status.time_valid ? "OK" : "TIME_INVALID";
    }
  }
#endif
}

WssTimeStatus wss_time_status() {
  bool tv = false;
  g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
  g_time_status.time_valid = tv;
  return g_time_status;
}

bool wss_time_set_epoch(uint32_t epoch_s, WssEventLogger* log) {
#if !WSS_FEATURE_RTC
  (void)epoch_s;
  (void)log;
  return false;
#else
  if (!g_time_status.pinmap_configured || !g_time_status.rtc_present) return false;
  g_rtc.adjust(DateTime(epoch_s));
  set_system_time_utc(epoch_s);

  bool tv = false;
  g_time_status.now_iso8601_utc = wss_time_now_iso8601_utc(tv);
  g_time_status.time_valid = tv;
  g_time_status.status = tv ? "OK" : "TIME_INVALID";

  if (log) {
    StaticJsonDocument<192> extra;
    extra["epoch_s"] = epoch_s;
    extra["status"] = g_time_status.status;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    log->log_info("time", "time_set", "RTC time set", &o);
  }
  return true;
#endif
}
