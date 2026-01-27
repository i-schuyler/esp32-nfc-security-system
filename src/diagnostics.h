// src/diagnostics.h
#pragma once
#include <Arduino.h>

struct WssBootInfo {
  String reset_reason;
  String chip_id_suffix; // last 4 hex chars of MAC
};

WssBootInfo wss_get_boot_info();
