// src/storage/storage_manager.cpp
// Role: Log persistence tiers (SD preferred, flash ring fallback) + status reporting for M2.

#include "storage_manager.h"

#include <ArduinoJson.h>
#include <time.h>

#include "../config/config_store.h"
#include "../config/pin_config.h"
#include "../logging/event_logger.h"
#include "flash_ring.h"
#include "time_manager.h"

#include "../logging/sha256_hex.h"

#include "version.h"

#if WSS_FEATURE_SD
#include <SPI.h>
#include <SdFat.h>
#endif

static WssStorageStatus g_status;
static WssEventLogger* g_log = nullptr;
static WssConfigStore* g_cfg = nullptr;

static WssFlashRing g_fallback;
static uint32_t g_last_poll_ms = 0;

#if WSS_FEATURE_SD
static SdFs g_sd;
static FsFile g_file;
static String g_last_day_key;
#endif

// M3: hash chaining state (best-effort, per active backend/day)
static bool g_hash_chain_enabled = false;
static String g_prev_hash;
static const char* kZeroHash64 = "0000000000000000000000000000000000000000000000000000000000000000";

// M3: retention enforcement pacing
static uint32_t g_last_retention_check_ms = 0;

static String date_key_utc(time_t t) {
  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);
  char buf[16];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_utc);
  return String(buf);
}

static String year_str_utc(time_t t) {
  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);
  char buf[8];
  strftime(buf, sizeof(buf), "%Y", &tm_utc);
  return String(buf);
}

static String month_str_utc(time_t t) {
  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);
  char buf[8];
  strftime(buf, sizeof(buf), "%m", &tm_utc);
  return String(buf);
}

static String clamp_prev_hash(const String& h) {
  if (h.length() == 64) return h;
  return String(kZeroHash64);
}

// Computes the chained hash for a JSON line.
// Rule: hash = SHA256( canonical_serialized_JSON_without_{hash,prev_hash} + prev_hash )
static bool apply_hash_chain_to_jsonl(const String& raw_line, const String& prev_hash,
                                      String& out_line, String& out_hash) {
  // Parse JSON
  DynamicJsonDocument d(768);
  DeserializationError de = deserializeJson(d, raw_line);
  if (de) return false;

  JsonObject root = d.as<JsonObject>();
  if (root.isNull()) return false;

  // Remove any existing hash fields before hashing.
  root.remove("hash");
  root.remove("prev_hash");

  String content;
  serializeJson(root, content);

  String input = content + clamp_prev_hash(prev_hash);
  out_hash = wss_sha256_hex_str(input);

  // Attach hash fields for persistence.
  root["prev_hash"] = clamp_prev_hash(prev_hash);
  root["hash"] = out_hash;

  out_line = "";
  serializeJson(root, out_line);
  return true;
}

#if WSS_FEATURE_SD
static const uint32_t kSdPollIntervalMs = 2000;

static String fs_type_string() {
  // SdFat defines EXFAT_TYPE when exFAT is enabled.
  uint8_t t = g_sd.fatType();
  if (t == 16) return "FAT16";
  if (t == 32) return "FAT32";
#ifdef EXFAT_TYPE
  if (t == EXFAT_TYPE) return "exFAT";
#endif
  if (t == 0) return "NONE";
  return String("FAT?") + String(t);
}

static void update_capacity_free() {
  if (!g_status.sd_mounted) {
    g_status.capacity_bytes = 0;
    g_status.free_bytes = 0;
    return;
  }

  if (g_sd.card()) {
    uint64_t sectors = (uint64_t)g_sd.card()->sectorCount();
    g_status.capacity_bytes = sectors * 512ULL;
  }

  // FsVolume provides freeClusterCount()/sectorsPerCluster() for FAT/exFAT.
  if (g_sd.vol()) {
    uint64_t free_clusters = g_sd.vol()->freeClusterCount();
    uint64_t spc = g_sd.vol()->sectorsPerCluster();
    g_status.free_bytes = free_clusters * spc * 512ULL;
  } else {
    g_status.free_bytes = 0;
  }
}

static bool ensure_sd_dirs(time_t now) {
  String y = year_str_utc(now);
  String m = month_str_utc(now);
  // Create /logs, /logs/YYYY, /logs/YYYY/MM
  if (!g_sd.exists("/logs")) {
    if (!g_sd.mkdir("/logs")) return false;
  }
  String ydir = String("/logs/") + y;
  if (!g_sd.exists(ydir.c_str())) {
    if (!g_sd.mkdir(ydir.c_str())) return false;
  }
  String mdir = ydir + String("/") + m;
  if (!g_sd.exists(mdir.c_str())) {
    if (!g_sd.mkdir(mdir.c_str())) return false;
  }
  return true;
}

static String log_path_for(time_t now) {
  String y = year_str_utc(now);
  String m = month_str_utc(now);
  String d = date_key_utc(now);
  return String("/logs/") + y + String("/") + m + String("/events_") + d + String(".txt");
}

static bool ensure_nfc_dir() {
  if (!g_sd.exists("/nfc")) {
    if (!g_sd.mkdir("/nfc")) return false;
  }
  return true;
}

static bool sd_read_last_hash(const String& path, String& out_hash) {
  out_hash = String(kZeroHash64);
  FsFile f = g_sd.open(path.c_str(), O_RDONLY);
  if (!f) return false;
  uint64_t sz = f.fileSize();
  // Read up to last 2048 bytes for the last JSONL line.
  const uint32_t kTail = 2048;
  uint32_t to_read = (sz > kTail) ? kTail : (uint32_t)sz;
  if (to_read == 0) {
    f.close();
    return true;
  }
  uint64_t start = (sz > kTail) ? (sz - kTail) : 0;
  f.seekSet(start);
  String tail;
  tail.reserve(to_read + 8);
  char buf[129];
  uint32_t remaining = to_read;
  while (remaining > 0) {
    uint32_t n = remaining > (sizeof(buf) - 1) ? (sizeof(buf) - 1) : remaining;
    int32_t got = f.read(buf, n);
    if (got <= 0) break;
    buf[got] = 0;
    tail.concat(String(buf));
    remaining -= (uint32_t)got;
  }
  f.close();

  // Find last non-empty line.
  int end = tail.length() - 1;
  while (end >= 0 && (tail[end] == '\n' || tail[end] == '\r')) end--;
  if (end < 0) return true;
  int start_line = tail.lastIndexOf('\n', end);
  if (start_line < 0) start_line = 0; else start_line += 1;
  String line = tail.substring(start_line, end + 1);
  line.trim();
  if (!line.length()) return true;

  DynamicJsonDocument d(768);
  DeserializationError de = deserializeJson(d, line);
  if (de) return false;
  String h = d["hash"] | String("");
  if (h.length() == 64) {
    out_hash = h;
  }
  return true;
}

static bool open_log_file_if_needed(time_t now) {
  String day = date_key_utc(now);
  if (g_status.active_log_path.length() > 0 && g_last_day_key == day && g_file) return true;

  if (g_file) g_file.close();
  g_status.active_log_path = "";

  if (!ensure_sd_dirs(now)) return false;

  String path = log_path_for(now);
  FsFile f = g_sd.open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND);
  if (!f) return false;
  bool is_new = (f.fileSize() == 0);
  g_file = f;
  g_last_day_key = day;
  g_status.active_log_path = path;

  // M3: initialize hash chain state for this day/file.
  if (g_hash_chain_enabled) {
    if (is_new) {
      g_prev_hash = String(kZeroHash64);
    } else {
      String last;
      if (!sd_read_last_hash(path, last)) last = String(kZeroHash64);
      g_prev_hash = clamp_prev_hash(last);
    }
    g_status.chain_head_hash = g_prev_hash;
  }

  // M3: write a schema-correct file header event on new files when hash chaining is enabled.
  if (g_hash_chain_enabled && is_new && g_log && g_cfg) {
    bool tv = false;
    String ts = wss_time_now_iso8601_utc(tv);
    StaticJsonDocument<384> hdr;
    hdr["ts"] = ts;
    hdr["seq"] = g_log->reserve_seq();
    hdr["severity"] = "info";
    hdr["source"] = "log";
    hdr["event_type"] = "file_header";
    hdr["msg"] = "file header";
    hdr["time_valid"] = tv;
    JsonObject extra = hdr.createNestedObject("extra");
    extra["firmware"] = WSS_FIRMWARE_VERSION;
    extra["log_schema_version"] = WSS_LOG_SCHEMA_VERSION;
    extra["config_schema_version"] = WSS_CONFIG_SCHEMA_VERSION;
    extra["nfc_record_version"] = WSS_NFC_RECORD_VERSION;
    extra["device_suffix"] = g_cfg->device_suffix();

    String base;
    serializeJson(hdr, base);
    String out_line;
    String out_hash;
    if (apply_hash_chain_to_jsonl(base, g_prev_hash, out_line, out_hash)) {
      g_file.println(out_line);
      g_file.flush();
      g_prev_hash = out_hash;
      g_status.chain_head_hash = g_prev_hash;
    }
  }
  return true;
}

bool wss_storage_write_allowlist(const String& payload, String& err) {
#if !WSS_FEATURE_SD
  err = "sd_disabled";
  return false;
#else
  if (!g_status.sd_mounted) {
    err = "sd_not_mounted";
    return false;
  }
  if (!ensure_nfc_dir()) {
    err = "nfc_dir_create_failed";
    return false;
  }
  FsFile f = g_sd.open("/nfc/allowlist.json", O_WRONLY | O_CREAT | O_TRUNC);
  if (!f) {
    err = "allowlist_open_failed";
    return false;
  }
  int32_t wrote = f.write(payload.c_str(), payload.length());
  f.close();
  if (wrote < 0 || (size_t)wrote != payload.length()) {
    err = "allowlist_write_failed";
    return false;
  }
  return true;
#endif
}

bool wss_storage_read_allowlist(String& payload, String& err) {
#if !WSS_FEATURE_SD
  err = "sd_disabled";
  return false;
#else
  if (!g_status.sd_mounted) {
    err = "sd_not_mounted";
    return false;
  }
  FsFile f = g_sd.open("/nfc/allowlist.json", O_RDONLY);
  if (!f) {
    err = "allowlist_missing";
    return false;
  }
  uint64_t sz = f.fileSize();
  if (sz > 16384) {
    f.close();
    err = "allowlist_too_large";
    return false;
  }
  payload = "";
  payload.reserve((size_t)sz + 8);
  char buf[129];
  while (f.available()) {
    int32_t got = f.read(buf, sizeof(buf) - 1);
    if (got <= 0) break;
    buf[got] = 0;
    payload.concat(String(buf));
  }
  f.close();
  if (!payload.length()) {
    err = "allowlist_empty";
    return false;
  }
  return true;
#endif
}

static bool sd_try_mount(const char* reason) {
  (void)reason;
  g_status.sd_mounted = false;
  g_status.fs_type = "";
  g_status.sd_status = "MISSING";

  // SPI init: allow explicit overrides, otherwise Arduino defaults.
  if (WSS_PIN_SPI_SCK >= 0 && WSS_PIN_SPI_MISO >= 0 && WSS_PIN_SPI_MOSI >= 0) {
    SPI.begin(WSS_PIN_SPI_SCK, WSS_PIN_SPI_MISO, WSS_PIN_SPI_MOSI, WSS_PIN_SD_CS);
  } else {
    SPI.begin();
  }

  SdSpiConfig cfg(WSS_PIN_SD_CS, SHARED_SPI);
  if (!g_sd.begin(cfg)) {
    g_status.sd_status = "ERROR";
    return false;
  }

  g_status.sd_mounted = true;
  g_status.fs_type = fs_type_string();
  g_status.sd_status = "OK";
  update_capacity_free();

  time_t now = time(nullptr);
  if (!open_log_file_if_needed(now)) {
    g_status.sd_status = "ERROR";
    g_status.sd_mounted = false;
    return false;
  }

  return true;
}

static void emit_sd_status_log(const char* msg) {
  if (!g_log) return;
  StaticJsonDocument<256> extra;
  extra["status"] = g_status.sd_status;
  extra["mounted"] = g_status.sd_mounted;
  extra["fs"] = g_status.fs_type;
  extra["capacity_bytes"] = (uint64_t)g_status.capacity_bytes;
  extra["free_bytes"] = (uint64_t)g_status.free_bytes;
  extra["backend"] = g_status.active_backend;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  g_log->log_info("sd", "sd_status", msg, &o);
}

// M3: best-effort retention enforcement (delete logs older than configured days).
static uint32_t g_last_retention_ms = 0;
static void enforce_retention_if_due() {
  if (!g_cfg || !g_log || !g_status.sd_mounted) return;
  // Only enforce retention when time is valid to avoid deleting based on 1970.
  auto tstat = wss_time_status();
  if (!tstat.time_valid) return;

  const uint32_t now_ms = millis();
  if ((uint32_t)(now_ms - g_last_retention_ms) < 60UL * 60UL * 1000UL) return; // hourly
  g_last_retention_ms = now_ms;

  int days = g_cfg->doc()["log_retention_days"] | 365;
  if (days < 7) days = 7;
  if (days > 3650) days = 3650;

  time_t now = time(nullptr);
  time_t cutoff = now - (time_t)days * 86400;
  String cutoff_key = date_key_utc(cutoff);

  uint32_t deleted = 0;
  FsFile logs = g_sd.open("/logs", O_RDONLY);
  if (!logs) return;

  FsFile year_dir;
  while (year_dir.openNext(&logs, O_RDONLY)) {
    if (!year_dir.isDir()) {
      year_dir.close();
      continue;
    }
    char yname[16] = {0};
    year_dir.getName(yname, sizeof(yname));
    String y = String(yname);

    FsFile month_dir;
    while (month_dir.openNext(&year_dir, O_RDONLY)) {
      if (!month_dir.isDir()) {
        month_dir.close();
        continue;
      }
      char mname[16] = {0};
      month_dir.getName(mname, sizeof(mname));
      String m = String(mname);

      FsFile file;
      while (file.openNext(&month_dir, O_RDONLY)) {
        if (file.isDir()) {
          file.close();
          continue;
        }
        char fname[48] = {0};
        file.getName(fname, sizeof(fname));
        String n = String(fname);
        file.close();

        if (!n.startsWith("events_") || !n.endsWith(".txt")) continue;
        if (n.length() < 21) continue; // events_YYYY-MM-DD.txt
        String date_key = n.substring(7, 17);
        if (date_key < cutoff_key) {
          String p = String("/logs/") + y + String("/") + m + String("/") + n;
          if (g_sd.remove(p.c_str())) {
            deleted++;
          }
        }
      }
      month_dir.close();
    }
    year_dir.close();
  }
  logs.close();

  if (deleted > 0) {
    StaticJsonDocument<192> extra;
    extra["deleted"] = deleted;
    extra["cutoff_date"] = cutoff_key;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_info("sd", "log_retention", "log retention enforced", &o);
  }
}
#endif

#if WSS_FEATURE_SD
static bool parse_log_date_key(const String& name, String& out_key) {
  if (!name.startsWith("events_") || !name.endsWith(".txt")) return false;
  if (name.length() < 21) return false; // events_YYYY-MM-DD.txt
  String key = name.substring(7, 17);
  if (key.length() != 10) return false;
  if (key[4] != '-' || key[7] != '-') return false;
  for (size_t i = 0; i < key.length(); i++) {
    if (i == 4 || i == 7) continue;
    if (key[i] < '0' || key[i] > '9') return false;
  }
  out_key = key;
  return true;
}

static void compute_range_keys(WssLogRange range, String& start_key, String& end_key) {
  start_key = "";
  end_key = "";
  if (range == WSS_LOG_RANGE_ALL) return;
  time_t now = time(nullptr);
  time_t start = now;
  if (range == WSS_LOG_RANGE_7D) {
    start = now - (time_t)6 * 86400;
  }
  start_key = date_key_utc(start);
  end_key = date_key_utc(now);
}

static bool log_in_range(const String& date_key, const String& start_key,
                         const String& end_key, WssLogRange range) {
  if (range == WSS_LOG_RANGE_ALL) return true;
  if (!start_key.length() || !end_key.length()) return false;
  return date_key >= start_key && date_key <= end_key;
}

static bool for_each_log_file(WssLogRange range,
                              bool (*cb)(const String& path, uint64_t size_bytes, void* ctx),
                              void* ctx, String& err) {
  if (!g_status.sd_mounted) {
    err = "sd_not_mounted";
    return false;
  }
  if (!g_sd.exists("/logs")) return true;
  FsFile logs = g_sd.open("/logs", O_RDONLY);
  if (!logs) {
    err = "logs_dir_open_failed";
    return false;
  }

  String start_key;
  String end_key;
  compute_range_keys(range, start_key, end_key);

  FsFile year_dir;
  while (year_dir.openNext(&logs, O_RDONLY)) {
    if (!year_dir.isDir()) {
      year_dir.close();
      continue;
    }
    char yname[16] = {0};
    year_dir.getName(yname, sizeof(yname));
    String y = String(yname);

    FsFile month_dir;
    while (month_dir.openNext(&year_dir, O_RDONLY)) {
      if (!month_dir.isDir()) {
        month_dir.close();
        continue;
      }
      char mname[16] = {0};
      month_dir.getName(mname, sizeof(mname));
      String m = String(mname);

      FsFile file;
      while (file.openNext(&month_dir, O_RDONLY)) {
        if (file.isDir()) {
          file.close();
          continue;
        }
        char fname[48] = {0};
        file.getName(fname, sizeof(fname));
        String n = String(fname);
        String date_key;
        if (!parse_log_date_key(n, date_key)) {
          file.close();
          continue;
        }
        if (!log_in_range(date_key, start_key, end_key, range)) {
          file.close();
          continue;
        }
        String path = String("/logs/") + y + String("/") + m + String("/") + n;
        uint64_t size_bytes = file.fileSize();
        file.close();
        if (cb && !cb(path, size_bytes, ctx)) {
          logs.close();
          return false;
        }
      }
      month_dir.close();
    }
    year_dir.close();
  }
  logs.close();
  return true;
}
#endif

void wss_storage_begin(WssConfigStore* cfg, WssEventLogger* log) {
  g_cfg = cfg;
  g_log = log;
  g_status = WssStorageStatus{};

  // M3: logging hash chain default-enabled; config can disable.
  if (g_cfg) {
    g_hash_chain_enabled = g_cfg->doc()["hash_chain_logs"] | true;
  } else {
    g_hash_chain_enabled = false;
  }
  g_status.hash_chain_enabled = g_hash_chain_enabled;
  g_prev_hash = String(kZeroHash64);
  g_status.chain_head_hash = g_prev_hash;
  g_status.write_fail_count = 0;
  g_status.last_write_ok = true;
  g_status.last_write_backend = "";
  g_status.last_write_error = "";

  g_fallback.begin();
  g_status.fallback_count = g_fallback.count();

#if !WSS_FEATURE_SD
  g_status.feature_enabled = false;
  g_status.pinmap_configured = false;
  g_status.sd_status = "DISABLED";
  g_status.fallback_active = true;
  g_status.active_backend = "flash";
  return;
#else
  g_status.feature_enabled = true;

  if (WSS_PIN_SD_CS < 0) {
    g_status.pinmap_configured = false;
    g_status.sd_status = "DISABLED";
    g_status.fallback_active = true;
    g_status.active_backend = "flash";
    if (g_log) g_log->log_warn("sd", "sd_disabled", "SD disabled: pin map not configured");
    return;
  }

  g_status.pinmap_configured = true;

  bool ok = sd_try_mount("boot");
  g_status.fallback_active = !ok;
  g_status.active_backend = ok ? "sd" : "flash";

  if (ok) {
    emit_sd_status_log("SD mounted");
  } else {
    emit_sd_status_log("SD not mounted; using fallback ring");
  }
#endif
}

void wss_storage_loop() {
  const uint32_t now_ms = millis();
  if ((uint32_t)(now_ms - g_last_poll_ms) < 2000) return;
  g_last_poll_ms = now_ms;

  g_status.fallback_count = g_fallback.count();

#if !WSS_FEATURE_SD
  g_status.active_backend = "flash";
  g_status.fallback_active = true;
  return;
#else
  if (!g_status.feature_enabled || !g_status.pinmap_configured) {
    g_status.active_backend = "flash";
    g_status.fallback_active = true;
    return;
  }

  bool mounted_before = g_status.sd_mounted;

  // Quick health check: try chdir to root.
  bool still_ok = false;
  if (g_status.sd_mounted) {
    still_ok = g_sd.chdir("/");
  }

  if (mounted_before && !still_ok) {
    g_status.sd_mounted = false;
    g_status.sd_status = "MISSING";
    g_status.fallback_active = true;
    g_status.active_backend = "flash";
    if (g_file) g_file.close();
    g_status.active_log_path = "";
    emit_sd_status_log("SD removed/unavailable; switched to fallback ring");
  }

  if (!g_status.sd_mounted) {
    // Attempt remount periodically.
    bool ok = sd_try_mount("poll");
    if (ok) {
      g_status.fallback_active = false;
      g_status.active_backend = "sd";
      emit_sd_status_log("SD remounted; switched to SD logging");
    }
  } else {
    update_capacity_free();
    time_t now = time(nullptr);
    (void)open_log_file_if_needed(now); // rotation scaffolding
    enforce_retention_if_due();
  }
#endif
}

WssStorageStatus wss_storage_status() {
  g_status.fallback_count = g_fallback.count();
  return g_status;
}

bool wss_storage_append_line(const String& line) {
  // M3: optionally attach hash chain fields (or explicit nulls when disabled).
  String out = line;
  bool transformed = false;

  if (g_hash_chain_enabled) {
    String hashed;
    String new_hash;
    if (apply_hash_chain_to_jsonl(line, g_prev_hash, hashed, new_hash)) {
      out = hashed;
      g_prev_hash = new_hash;
      g_status.chain_head_hash = g_prev_hash;
      transformed = true;
    }
  } else {
    // Best-effort: explicit nulls for clarity when hashing is disabled.
    DynamicJsonDocument d(768);
    if (!deserializeJson(d, line)) {
      JsonObject root = d.as<JsonObject>();
      if (!root.isNull()) {
        root["prev_hash"] = nullptr;
        root["hash"] = nullptr;
        String tmp;
        serializeJson(root, tmp);
        out = tmp;
        transformed = true;
      }
    }
  }

  // Prefer SD if mounted.
#if WSS_FEATURE_SD
  if (g_status.feature_enabled && g_status.pinmap_configured && g_status.sd_mounted && g_file) {
    size_t n = g_file.println(out);
    g_file.flush();
    g_status.last_write_backend = "sd";
    g_status.last_write_ok = (n > 0);
    g_status.last_write_error = g_status.last_write_ok ? "" : "sd_write_failed";
    if (n > 0) return true;

    // If write fails, fall back.
    g_status.write_fail_count++;
    g_status.sd_mounted = false;
    g_status.sd_status = "ERROR";
    g_status.fallback_active = true;
    g_status.active_backend = "flash";
    if (g_file) g_file.close();
    g_status.active_log_path = "";
    if (g_log) g_log->log_warn("sd", "sd_status", "SD write failed; switched to fallback ring");
  }
#endif

  bool ok = g_fallback.append(out);
  g_status.last_write_backend = "flash";
  g_status.last_write_ok = ok;
  g_status.last_write_error = ok ? "" : "flash_ring_append_failed";
  if (!ok) {
    g_status.write_fail_count++;
    // Cannot log via logger here (would recurse). Make failure visible via status + serial.
    Serial.println("[storage] ERROR: flash ring append failed (logs may be lost)");
  }
  return ok;
}

size_t wss_storage_read_fallback(String* out, size_t max_items) {
  return g_fallback.read_recent(out, max_items);
}

bool wss_storage_list_log_files(WssLogFileInfo* out, size_t max_items, size_t& out_count,
                                bool& out_truncated, String& err) {
  out_count = 0;
  out_truncated = false;
  err = "";
#if !WSS_FEATURE_SD
  err = "sd_disabled";
  return false;
#else
  if (!g_status.sd_mounted) {
    err = "sd_not_mounted";
    return false;
  }
  struct ListCtx {
    WssLogFileInfo* out;
    size_t max_items;
    size_t count;
    bool truncated;
  };
  ListCtx ctx{out, max_items, 0, false};
  auto cb = [](const String& path, uint64_t size_bytes, void* ptr) -> bool {
    ListCtx* c = static_cast<ListCtx*>(ptr);
    if (c->count < c->max_items) {
      c->out[c->count].name = path;
      c->out[c->count].size_bytes = size_bytes;
      c->count++;
    } else {
      c->truncated = true;
    }
    return true;
  };
  bool ok = for_each_log_file(WSS_LOG_RANGE_ALL, cb, &ctx, err);
  out_count = ctx.count;
  out_truncated = ctx.truncated;
  return ok;
#endif
}

bool wss_storage_log_bytes(WssLogRange range, uint64_t& total_bytes, size_t& file_count,
                           String& err) {
  total_bytes = 0;
  file_count = 0;
  err = "";
#if !WSS_FEATURE_SD
  err = "sd_disabled";
  return false;
#else
  if (!g_status.sd_mounted) {
    err = "sd_not_mounted";
    return false;
  }
  struct SizeCtx {
    uint64_t total_bytes;
    size_t file_count;
  };
  SizeCtx size_ctx{0, 0};
  auto size_cb = [](const String& path, uint64_t size_bytes, void* ptr) -> bool {
    (void)path;
    SizeCtx* c = static_cast<SizeCtx*>(ptr);
    c->total_bytes += size_bytes;
    c->file_count++;
    return true;
  };
  bool ok = for_each_log_file(range, size_cb, &size_ctx, err);
  if (!ok) return false;
  total_bytes = size_ctx.total_bytes;
  file_count = size_ctx.file_count;
  return true;
#endif
}

bool wss_storage_stream_logs(WssLogRange range, Stream& out, uint32_t max_bytes,
                             size_t& bytes_sent, String& err) {
  bytes_sent = 0;
  err = "";
#if !WSS_FEATURE_SD
  err = "sd_disabled";
  return false;
#else
  uint64_t total_bytes = 0;
  size_t file_count = 0;
  if (!wss_storage_log_bytes(range, total_bytes, file_count, err)) return false;
  if (total_bytes > max_bytes) {
    err = "too_large";
    return false;
  }

  struct StreamCtx {
    Stream* out;
    size_t bytes_sent;
    String err;
  };
  StreamCtx stream_ctx{&out, 0, ""};
  auto stream_cb = [](const String& path, uint64_t size_bytes, void* ptr) -> bool {
    (void)size_bytes;
    StreamCtx* c = static_cast<StreamCtx*>(ptr);
    FsFile f = g_sd.open(path.c_str(), O_RDONLY);
    if (!f) {
      c->err = "log_open_failed";
      return false;
    }
    uint8_t buf[1024];
    while (f.available()) {
      int32_t got = f.read(buf, sizeof(buf));
      if (got <= 0) break;
      size_t wrote = c->out->write(buf, (size_t)got);
      c->bytes_sent += wrote;
      if (wrote != (size_t)got) {
        c->err = "log_stream_failed";
        f.close();
        return false;
      }
    }
    f.close();
    return true;
  };
  bool ok = for_each_log_file(range, stream_cb, &stream_ctx, err);
  if (!ok) {
    if (stream_ctx.err.length()) err = stream_ctx.err;
    return false;
  }
  bytes_sent = stream_ctx.bytes_sent;
  return true;
#endif
}
