// src/storage/storage_manager.h
// Role: Log persistence tiers (SD preferred, flash ring fallback) + status reporting for M2.
#pragma once

#include <Arduino.h>

class WssConfigStore;
class WssEventLogger;

struct WssStorageStatus {
  bool feature_enabled = false;   // compile-time flag (WSS_FEATURE_SD)
  bool pinmap_configured = false;
  bool sd_mounted = false;

  String sd_status; // OK|MISSING|ERROR|DISABLED
  String fs_type;   // FAT16|FAT32|exFAT|...

  uint64_t capacity_bytes = 0;
  uint64_t free_bytes = 0;

  bool fallback_active = true;
  uint32_t fallback_count = 0;

  // M3: log hashing + write diagnostics
  bool hash_chain_enabled = false;
  String chain_head_hash; // last computed hash (best-effort)
  uint32_t write_fail_count = 0;
  bool last_write_ok = true;
  String last_write_backend; // sd|flash
  String last_write_error;   // empty when OK

  String active_backend; // sd|flash
  String active_log_path; // when sd backend
};

void wss_storage_begin(WssConfigStore* cfg, WssEventLogger* log);
void wss_storage_loop();
WssStorageStatus wss_storage_status();

// Append a single JSONL line to the active backend (SD when available, else flash ring).
bool wss_storage_append_line(const String& line);

// Export most-recent flash ring items (newest-last). Returns number written.
size_t wss_storage_read_fallback(String* out, size_t max_items);

// NFC allowlist persistence (SD preferred; returns false if SD unavailable).
bool wss_storage_write_allowlist(const String& payload, String& err);
bool wss_storage_read_allowlist(String& payload, String& err);
