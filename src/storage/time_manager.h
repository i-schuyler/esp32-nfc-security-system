// src/storage/time_manager.h
// Role: RTC-backed time source (DS3231) + time validity reporting for M2.
#pragma once

#include <Arduino.h>

class WssEventLogger;

struct WssTimeStatus {
  bool feature_enabled = false;
  bool pinmap_configured = false;
  bool rtc_present = false;
  bool time_valid = false;
  String now_iso8601_utc;
  String status;
};

void wss_time_begin(WssEventLogger* log);
void wss_time_loop();
WssTimeStatus wss_time_status();
String wss_time_now_iso8601_utc(bool& time_valid);
bool wss_time_set_epoch(uint32_t epoch_s, WssEventLogger* log);
