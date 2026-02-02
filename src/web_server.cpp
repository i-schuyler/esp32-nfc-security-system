// src/web_server.cpp
// Role: Embedded HTTP server (SPA + JSON API) for V1.
// M1 adds: Setup Wizard, ConfigStore endpoints, Admin Config Mode gating, Factory Restore.

#include "web_server.h"

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_system.h>

#include "diagnostics.h"
#include "flash_fs.h"
#include "version.h"

#include "config/config_store.h"
#include "logging/event_logger.h"
#include "storage/time_manager.h"
#include "storage/storage_manager.h"
#include "wifi/wifi_manager.h"
#include "nfc/nfc_allowlist.h"

// M4: explicit alarm state machine + outputs
#include "state_machine/state_machine.h"
#include "outputs/output_manager.h"

// M5: sensors abstraction + status
#include "sensors/sensor_manager.h"
// M6: NFC health + scan events (slice 0)
#include "nfc/nfc_manager.h"

static WebServer server(80);
static WssConfigStore* g_cfg = nullptr;
static WssEventLogger* g_log = nullptr;
static const uint32_t kLogDownloadMaxBytes = 512 * 1024;
static const size_t kMaxLogListItems = 128;
static const size_t kMaxFallbackItems = 64;

struct AdminSession {
  bool active = false;
  uint32_t expires_at_ms = 0;
  String token;

  void clear() {
    active = false;
    expires_at_ms = 0;
    token = "";
  }

  bool expired() const {
    if (!active) return true;
    return (int32_t)(millis() - expires_at_ms) >= 0;
  }

  uint32_t remaining_s() const {
    if (expired()) return 0;
    return (expires_at_ms - millis()) / 1000UL;
  }
};

static AdminSession g_admin;

static bool admin_required(const char* action_name) {
  if (!g_cfg || !g_log) return false;
  if (g_admin.expired()) {
    g_admin.clear();
  }
  if (!g_admin.active) {
    StaticJsonDocument<192> extra;
    extra["action"] = action_name;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_warn("ui", "admin_required", String("admin_required:") + action_name, &o);
    server.send(403, "application/json", "{\"error\":\"admin_required\"}");
    return false;
  }
  String got = server.header("X-Admin-Token");
  if (got != g_admin.token) {
    StaticJsonDocument<192> extra;
    extra["action"] = action_name;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_warn("ui", "admin_token_invalid", String("admin_token_invalid:") + action_name, &o);
    server.send(403, "application/json", "{\"error\":\"admin_token_invalid\"}");
    return false;
  }
  return true;
}

static void send_json(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static void handle_status() {
  // M5: sensor status adds nested arrays; keep headroom.
  StaticJsonDocument<2048> doc;
  auto boot = wss_get_boot_info();
  auto wifi = wss_wifi_status();
  auto tstat = wss_time_status();
  auto sstat = wss_storage_status();
  auto sm = wss_state_status();
  auto out = wss_outputs_status();

  doc["firmware_name"] = WSS_FIRMWARE_NAME;
  doc["firmware_version"] = WSS_FIRMWARE_VERSION;
  doc["config_schema_version"] = WSS_CONFIG_SCHEMA_VERSION;
  doc["log_schema_version"] = WSS_LOG_SCHEMA_VERSION;
  doc["nfc_record_version"] = WSS_NFC_RECORD_VERSION;

  doc["reset_reason"] = boot.reset_reason;
  doc["device_suffix"] = boot.chip_id_suffix;

  doc["wifi_mode"] = wifi.mode;
  doc["wifi_ssid"] = wifi.ssid;
  doc["ip"] = wifi.ip;
  doc["rssi"] = wifi.rssi;

  doc["flash_fs_ok"] = wss_flash_fs_has_index();

  // M2: time/RTC status (observable, offline-first).
  {
    JsonObject t = doc.createNestedObject("time");
    t["status"] = tstat.status;
    t["rtc_present"] = tstat.rtc_present;
    t["time_valid"] = tstat.time_valid;
    t["pinmap_configured"] = tstat.pinmap_configured;
    t["now_iso8601_utc"] = tstat.now_iso8601_utc;
  }

  // M2: storage tiers status (SD preferred, flash ring fallback).
  {
    JsonObject s = doc.createNestedObject("storage");
    s["sd_status"] = sstat.sd_status;
    s["sd_mounted"] = sstat.sd_mounted;
    // UI-friendly aliases (append-only) used by the embedded SPA.
    s["status"] = sstat.sd_status;
    s["pinmap_configured"] = sstat.pinmap_configured;
    s["fs_type"] = sstat.fs_type;
    s["capacity_bytes"] = (uint64_t)sstat.capacity_bytes;
    s["free_bytes"] = (uint64_t)sstat.free_bytes;
    s["free_mb"] = (double)sstat.free_bytes / (1024.0 * 1024.0);
    s["active_backend"] = sstat.active_backend;
    s["active_log_path"] = sstat.active_log_path;
    s["fallback_active"] = sstat.fallback_active;
    s["fallback_count"] = sstat.fallback_count;

    // M3: tamper-aware log diagnostics
    s["hash_chain_enabled"] = sstat.hash_chain_enabled;
    s["chain_head_hash"] = sstat.chain_head_hash;
    s["write_fail_count"] = sstat.write_fail_count;
    s["last_write_ok"] = sstat.last_write_ok;
    s["last_write_backend"] = sstat.last_write_backend;
    s["last_write_error"] = sstat.last_write_error;
  }

  // M4: explicit state machine
  doc["state"] = sm.state;
  doc["state_machine_active"] = sm.state_machine_active;
  {
    JsonObject lt = doc.createNestedObject("last_transition");
    lt["ts"] = sm.last_transition.ts;
    lt["time_valid"] = sm.last_transition.time_valid;
    lt["from"] = sm.last_transition.from;
    lt["to"] = sm.last_transition.to;
    lt["reason"] = sm.last_transition.reason;
  }
  doc["silenced_remaining_s"] = sm.silenced_remaining_s;
  {
    JsonObject f = doc.createNestedObject("fault");
    f["active"] = sm.fault.active;
    if (sm.fault.code.length()) f["code"] = sm.fault.code;
    if (sm.fault.detail.length()) f["detail"] = sm.fault.detail;
  }
  {
    JsonObject o = doc.createNestedObject("outputs");
    o["horn_pin_configured"] = out.horn_pin_configured;
    o["light_pin_configured"] = out.light_pin_configured;
    o["horn_enabled_cfg"] = out.horn_enabled_cfg;
    o["light_enabled_cfg"] = out.light_enabled_cfg;
    o["horn_active"] = out.horn_active;
    o["light_active"] = out.light_active;
    o["horn_pattern"] = out.horn_pattern;
    o["light_pattern"] = out.light_pattern;
    o["silenced_light_pattern"] = out.silenced_light_pattern;
    o["applied_for_state"] = out.applied_for_state;
  }

  // M5: sensors status (append-only)
  {
    JsonObject sens = doc.createNestedObject("sensors");
    wss_sensors_write_status_json(sens);
  }

  // M6: NFC status (append-only, slice 0 health only)
  {
    JsonObject nfc = doc.createNestedObject("nfc");
    wss_nfc_write_status_json(nfc);
  }

  bool setup_done = g_cfg ? g_cfg->setup_completed() : false;
  doc["setup_required"] = !setup_done;
  doc["setup_last_step"] = g_cfg ? g_cfg->setup_last_step() : "welcome";

  if (g_admin.expired()) g_admin.clear();
  bool gate_required = wss_nfc_admin_gate_required();
  bool eligible = gate_required && wss_nfc_admin_eligible_active();
  uint32_t eligible_remaining_s = eligible ? wss_nfc_admin_eligible_remaining_s() : 0;
  const char* admin_mode = "off";
  uint32_t admin_remaining_s = 0;
  if (g_admin.active) {
    admin_mode = "authenticated";
    admin_remaining_s = g_admin.remaining_s();
  } else if (eligible) {
    admin_mode = "eligible";
    admin_remaining_s = eligible_remaining_s;
  }
  doc["admin_mode_active"] = g_admin.active;
  doc["admin_mode_remaining_s"] = admin_remaining_s;
  doc["admin_mode"] = admin_mode;

  send_json(200, doc);
}

static void handle_events() {
  size_t limit = 20;
  if (server.hasArg("limit")) {
    limit = (size_t)server.arg("limit").toInt();
    if (limit > 60) limit = 60;
  }
  DynamicJsonDocument out(8192);
  if (g_log) g_log->recent_events(out, limit);
  send_json(200, out);
}

static bool parse_log_range(const String& range, WssLogRange& out) {
  if (range == "today") {
    out = WSS_LOG_RANGE_TODAY;
    return true;
  }
  if (range == "7d") {
    out = WSS_LOG_RANGE_7D;
    return true;
  }
  if (range == "all") {
    out = WSS_LOG_RANGE_ALL;
    return true;
  }
  return false;
}

static void log_logs_event(const char* severity, const char* event_type, const char* msg,
                           const char* range, uint64_t bytes, size_t file_count,
                           const char* reason) {
  if (!g_log) return;
  StaticJsonDocument<192> extra;
  if (range && range[0]) extra["range"] = range;
  if (bytes) extra["bytes"] = bytes;
  if (file_count) extra["file_count"] = (uint32_t)file_count;
  if (reason && reason[0]) extra["reason"] = reason;
  JsonObjectConst o = extra.as<JsonObjectConst>();
  if (severity && strcmp(severity, "warn") == 0) {
    g_log->log_warn("ui", event_type, msg, &o);
  } else if (severity && strcmp(severity, "error") == 0) {
    g_log->log_error("ui", event_type, msg, &o);
  } else {
    g_log->log_info("ui", event_type, msg, &o);
  }
}

static void handle_logs_list() {
  if (!admin_required("logs_list")) return;
  WssStorageStatus sstat = wss_storage_status();
  if (!sstat.sd_mounted) {
    StaticJsonDocument<128> doc;
    doc["sd_missing"] = true;
    doc["flash_snapshot"] = true;
    send_json(200, doc);
    log_logs_event("info", "logs_list", "logs list requested (flash snapshot only)",
      "", 0, 0, "sd_missing");
    return;
  }

  WssLogFileInfo items[kMaxLogListItems];
  size_t count = 0;
  bool truncated = false;
  String err;
  if (!wss_storage_list_log_files(items, kMaxLogListItems, count, truncated, err)) {
    server.send(500, "application/json", "{\"error\":\"log_list_failed\"}");
    log_logs_event("error", "logs_list_failed", "logs list failed", "", 0, 0, err.c_str());
    return;
  }

  DynamicJsonDocument doc(16384);
  doc["sd_missing"] = false;
  doc["truncated"] = truncated;
  JsonArray files = doc.createNestedArray("files");
  for (size_t i = 0; i < count; i++) {
    JsonObject f = files.createNestedObject();
    f["name"] = items[i].name;
    f["size_bytes"] = (uint64_t)items[i].size_bytes;
  }
  send_json(200, doc);
  log_logs_event("info", "logs_list", "logs list requested", "",
    0, count, truncated ? "truncated" : "");
}

static const char* kFlashFallbackHeader =
  "# FLASH_FALLBACK_LOG_SNAPSHOT (most recent entries)\n";

static bool build_flash_fallback_snapshot(String* lines, size_t max_items, size_t& count,
                                          size_t& total_bytes, String& err) {
  err = "";
  count = wss_storage_read_fallback(lines, max_items);
  total_bytes = strlen(kFlashFallbackHeader);
  for (size_t i = 0; i < count; i++) {
    total_bytes += lines[i].length() + 1;
  }
  if (total_bytes > kLogDownloadMaxBytes) {
    err = "too_large";
    return false;
  }
  return true;
}

static void write_flash_fallback_snapshot(const String* lines, size_t count, size_t& bytes_sent) {
  bytes_sent = 0;
  WiFiClient client = server.client();
  client.write(reinterpret_cast<const uint8_t*>(kFlashFallbackHeader),
    strlen(kFlashFallbackHeader));
  bytes_sent += strlen(kFlashFallbackHeader);
  for (size_t i = 0; i < count; i++) {
    client.write(reinterpret_cast<const uint8_t*>(lines[i].c_str()), lines[i].length());
    client.write(reinterpret_cast<const uint8_t*>("\n"), 1);
    bytes_sent += lines[i].length() + 1;
  }
}

static void handle_logs_download() {
  if (!admin_required("logs_download")) return;
  String range_str = server.hasArg("range") ? server.arg("range") : String("");
  WssLogRange range = WSS_LOG_RANGE_TODAY;
  if (!parse_log_range(range_str, range)) {
    server.send(400, "application/json", "{\"error\":\"bad_range\"}");
    log_logs_event("warn", "logs_download_failed", "logs download failed: bad range",
      range_str.c_str(), 0, 0, "bad_range");
    return;
  }
  log_logs_event("info", "logs_download_start", "logs download started",
    range_str.c_str(), 0, 0, "");

  uint64_t total_bytes = 0;
  size_t file_count = 0;
  String err;
  WssStorageStatus sstat = wss_storage_status();
  String fallback_lines[kMaxFallbackItems];
  size_t fallback_count = 0;
  size_t fallback_total = 0;
  if (sstat.sd_mounted) {
    if (!wss_storage_log_bytes(range, total_bytes, file_count, err)) {
      server.send(500, "application/json", "{\"error\":\"log_download_failed\"}");
      log_logs_event("error", "logs_download_failed", "logs download failed",
        range_str.c_str(), 0, 0, err.c_str());
      return;
    }
    if (total_bytes > kLogDownloadMaxBytes) {
      server.send(409, "application/json",
        "{\"error\":\"too_large\",\"message\":\"Too large to download. Choose a shorter range.\"}");
      log_logs_event("warn", "logs_download_refused", "logs download refused: too large",
        range_str.c_str(), total_bytes, file_count, "too_large");
      return;
    }
  } else {
    if (!build_flash_fallback_snapshot(fallback_lines, kMaxFallbackItems,
          fallback_count, fallback_total, err)) {
      server.send(409, "application/json",
        "{\"error\":\"too_large\",\"message\":\"Too large to download. Choose a shorter range.\"}");
      log_logs_event("warn", "logs_download_refused", "logs download refused: too large",
        range_str.c_str(), 0, 0, "too_large");
      return;
    }
  }

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Content-Disposition",
    String("attachment; filename=\"logs_") + range_str + String(".txt\""));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain", "");

  size_t bytes_sent = 0;
  if (!sstat.sd_mounted) {
    write_flash_fallback_snapshot(fallback_lines, fallback_count, bytes_sent);
    log_logs_event("info", "logs_download_ok", "logs download complete (flash fallback)",
      range_str.c_str(), bytes_sent, 0, "flash_fallback");
    return;
  }

  auto client = server.client();
  if (!wss_storage_stream_logs(range, client, kLogDownloadMaxBytes, bytes_sent, err)) {
    log_logs_event("error", "logs_download_failed", "logs download failed",
      range_str.c_str(), 0, 0, err.c_str());
    return;
  }
  log_logs_event("info", "logs_download_ok", "logs download complete",
    range_str.c_str(), bytes_sent, file_count, "");
}

static void handle_admin_status() {
  StaticJsonDocument<256> doc;
  if (g_admin.expired()) g_admin.clear();
  bool gate_required = wss_nfc_admin_gate_required();
  bool eligible = gate_required && wss_nfc_admin_eligible_active();
  uint32_t eligible_remaining_s = eligible ? wss_nfc_admin_eligible_remaining_s() : 0;
  const char* admin_mode = "off";
  uint32_t admin_remaining_s = 0;
  if (g_admin.active) {
    admin_mode = "authenticated";
    admin_remaining_s = g_admin.remaining_s();
  } else if (eligible) {
    admin_mode = "eligible";
    admin_remaining_s = eligible_remaining_s;
  }
  doc["active"] = g_admin.active;
  doc["remaining_s"] = admin_remaining_s;
  doc["mode"] = admin_mode;
  send_json(200, doc);
}

static void handle_admin_login() {
  if (!g_cfg || !g_log) {
    server.send(500, "application/json", "{\"error\":\"cfg_unavailable\"}");
    return;
  }

  StaticJsonDocument<512> body;
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }

  String password = body["password"] | String("");
  if (!g_cfg->admin_password_set()) {
    server.send(409, "application/json", "{\"error\":\"admin_password_not_set\"}");
    return;
  }
  if (wss_nfc_admin_gate_required() && !wss_nfc_admin_eligible_active()) {
    StaticJsonDocument<96> extra;
    extra["reason"] = "nfc_required";
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log->log_warn("ui", "admin_login_blocked", "admin login blocked: nfc required", &o);
    server.send(403, "application/json", "{\"error\":\"admin_nfc_required\"}");
    return;
  }

  if (!g_cfg->verify_admin_password(password)) {
    g_log->log_warn("ui", "admin_login_failed", "admin login failed");
    server.send(403, "application/json", "{\"error\":\"invalid_password\"}");
    return;
  }

  uint32_t timeout_s = g_cfg->doc()["admin_mode_timeout_s"] | 600;
  g_admin.active = true;
  g_admin.expires_at_ms = millis() + timeout_s * 1000UL;

  // Simple token (RAM only). Token must be sent as X-Admin-Token.
  uint32_t r0 = esp_random();
  uint32_t r1 = esp_random();
  char buf[24];
  snprintf(buf, sizeof(buf), "%08lx%08lx", (unsigned long)r0, (unsigned long)r1);
  g_admin.token = String(buf);

  StaticJsonDocument<256> out;
  out["token"] = g_admin.token;
  out["expires_in_s"] = timeout_s;
  send_json(200, out);

  g_log->log_info("ui", "admin_mode_entered", "admin mode entered");
}

static void handle_admin_logout() {
  if (g_admin.expired()) g_admin.clear();
  g_admin.clear();
  if (g_log) g_log->log_info("ui", "admin_mode_exited", "admin mode exited");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_admin_eligible_clear() {
  wss_nfc_admin_eligible_clear("api_clear");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_time_set() {
  // Time setting is allowed in two cases:
  // 1) During setup wizard (before completion), without admin.
  // 2) After setup, only in admin mode.
  if (!g_cfg || !g_log) {
    server.send(500, "application/json", "{\"error\":\"cfg_unavailable\"}");
    return;
  }
  if (g_cfg->setup_completed()) {
    if (!admin_required("time_set")) return;
  }

  StaticJsonDocument<256> body;
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }
  uint32_t epoch_s = body["epoch_s"] | 0;
  if (!epoch_s) {
    server.send(400, "application/json", "{\"error\":\"missing_epoch_s\"}");
    return;
  }
  if (!wss_time_set_epoch(epoch_s, g_log)) {
    server.send(409, "application/json", "{\"error\":\"rtc_set_failed\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_wizard_status() {
  StaticJsonDocument<256> doc;
  bool setup_done = g_cfg ? g_cfg->setup_completed() : false;
  doc["required"] = !setup_done;
  doc["completed"] = setup_done;
  doc["last_step"] = g_cfg ? g_cfg->setup_last_step() : "welcome";
  send_json(200, doc);
}

static void handle_wizard_set_step() {
  if (!g_cfg || !g_log) {
    server.send(500, "application/json", "{\"error\":\"cfg_unavailable\"}");
    return;
  }

  StaticJsonDocument<768> body;
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }

  // Wizard endpoints are allowed even without admin mode, but only before completion.
  if (g_cfg->setup_completed()) {
    if (!admin_required("wizard_set_step")) return;
  }

  String step = body["step"] | String("");
  JsonObject payload = body["data"].as<JsonObject>();

  // Apply wizard fields.
  String err;
  bool ok = true;
  if (!step.length()) {
    server.send(400, "application/json", "{\"error\":\"missing_step\"}");
    return;
  }

  // Track last step.
  StaticJsonDocument<128> keys;
  JsonArray changed = keys.to<JsonArray>();

  if (!g_cfg->wizard_set("setup_last_step", step.c_str(), err)) {
    ok = false;
  } else {
    changed.add("setup_last_step");
  }

  if (payload) {
    for (JsonPair kv : payload) {
      String k = kv.key().c_str();
      // M2: allow time setting via wizard without storing it in config.
      if (k == "rtc_set_epoch_s") {
        uint32_t epoch_s = kv.value() | 0;
        if (!epoch_s) {
          ok = false;
          err = "rtc_set_bad_epoch";
        } else if (!wss_time_set_epoch(epoch_s, g_log)) {
          ok = false;
          err = "rtc_set_failed";
        }
        continue;
      }
      if (k == "admin_web_password") {
        String e;
        if (g_cfg->wizard_set_variant("admin_web_password", kv.value(), e)) {
          changed.add("admin_web_password");
        } else {
          ok = false;
          err = e;
        }
        continue;
      }
      String e;
      if (g_cfg->wizard_set_variant(k.c_str(), kv.value(), e)) {
        changed.add(k);
      } else {
        // Unknown or invalid keys are rejected.
        ok = false;
        err = e;
      }
    }
  }

  if (!ok) {
    server.send(400, "application/json", String("{\"error\":\"") + err + "\"}");
    return;
  }

  String save_err;
  g_cfg->ensure_runtime_defaults();
  if (!g_cfg->save(save_err)) {
    server.send(500, "application/json", "{\"error\":\"save_failed\"}");
    return;
  }

  g_log->log_config_change("ui", changed);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_wizard_complete() {
  if (!g_cfg || !g_log) {
    server.send(500, "application/json", "{\"error\":\"cfg_unavailable\"}");
    return;
  }

  // Ensure required minimums. (M1: primary sensor must be motion OR door enabled.)
  bool motion = g_cfg->doc()["motion_enabled"] | true;
  bool door = g_cfg->doc()["door_enabled"] | false;
  if (!motion && !door) {
    g_log->log_warn("ui", "wizard_blocked", "wizard completion blocked: no primary sensor enabled");
    server.send(409, "application/json", "{\"error\":\"primary_sensor_required\"}");
    return;
  }

  if (!g_cfg->admin_password_set()) {
    server.send(409, "application/json", "{\"error\":\"admin_password_required\"}");
    return;
  }

  if (g_cfg->ap_password_is_default()) {
    g_log->log_warn("ui", "wizard_blocked", "wizard completion blocked: AP password still default");
    server.send(409, "application/json", "{\"error\":\"ap_password_change_required\"}");
    return;
  }

  String err;
  g_cfg->wizard_set("setup_completed", true, err);
  g_cfg->wizard_set("setup_last_step", "complete", err);
  String save_err;
  if (!g_cfg->save(save_err)) {
    server.send(500, "application/json", "{\"error\":\"save_failed\"}");
    return;
  }
  g_log->log_info("ui", "wizard_completed", "setup wizard completed");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_config_get() {
  if (!admin_required("config_get")) return;
  DynamicJsonDocument out(4096);
  g_cfg->to_redacted_json(out);
  send_json(200, out);
}

static void handle_config_post() {
  if (!admin_required("config_post")) return;
  if (!g_cfg || !g_log) {
    server.send(500, "application/json", "{\"error\":\"cfg_unavailable\"}");
    return;
  }

  DynamicJsonDocument body(2048);
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }

  DynamicJsonDocument changed(256);
  JsonArray changed_keys = changed.to<JsonArray>();
  String err;
  bool did_change = g_cfg->apply_patch(body.as<JsonObjectConst>(), err, changed_keys);
  if (err.length()) {
    server.send(400, "application/json", String("{\"error\":\"") + err + "\"}");
    return;
  }

  if (did_change) {
    String save_err;
    if (!g_cfg->save(save_err)) {
      server.send(500, "application/json", "{\"error\":\"save_failed\"}");
      return;
    }
    g_log->log_config_change("ui", changed_keys);
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_factory_restore() {
  if (!admin_required("factory_restore")) return;

  StaticJsonDocument<512> body;
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }

  String phrase = body["confirm_phrase"] | String("");
  uint32_t hold_ms = body["hold_ms"] | 0;
  if (phrase != "FACTORY RESTORE" || hold_ms < 3000) {
    server.send(409, "application/json", "{\"error\":\"confirm_required\"}");
    return;
  }

  String err;
  if (!g_cfg->factory_reset(err)) {
    server.send(500, "application/json", "{\"error\":\"restore_failed\"}");
    return;
  }

  // Explicit allowlist reset hook (stubbed until NFC provisioning milestone).
  wss_nfc_allowlist_factory_reset(*g_log);

  g_admin.clear();
  g_log->log_warn("ui", "factory_restore", "factory restore completed");
  server.send(200, "application/json", "{\"ok\":true}");
}

static bool web_controls_enabled() {
  if (!g_cfg) return false;
  JsonObject root = g_cfg->doc().as<JsonObject>();
  if (!root.containsKey("control_web_enabled")) return true;
  if (root["control_web_enabled"].is<bool>()) return root["control_web_enabled"].as<bool>();
  return true;
}

static void handle_control_action(const char* which) {
  if (!web_controls_enabled()) {
    server.send(409, "application/json", "{\"error\":\"web_control_disabled\"}");
    if (g_log) g_log->log_warn("ui", "web_control_disabled", String("control rejected:") + which);
    return;
  }

  bool ok = false;
  if (String(which) == "arm") ok = wss_state_arm("web_arm");
  else if (String(which) == "disarm") ok = wss_state_disarm("web_disarm");
  else if (String(which) == "silence") ok = wss_state_silence("web_silence");

  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  doc["action"] = which;
  doc["state"] = wss_state_status().state;

  if (!ok) {
    doc["error"] = "invalid_transition_or_fault";
    send_json(409, doc);
  } else {
    send_json(200, doc);
  }
}

static void handle_nfc_provision_start() {
  if (!admin_required("nfc_provision_start")) return;
  StaticJsonDocument<256> body;
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }
  String mode = body["mode"] | String("");
  if (!mode.length()) {
    server.send(400, "application/json", "{\"error\":\"missing_mode\"}");
    return;
  }
  if (!wss_nfc_provision_start(mode.c_str())) {
    server.send(409, "application/json", "{\"error\":\"provision_start_failed\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_nfc_provision_mode() {
  if (!admin_required("nfc_provision_mode")) return;
  StaticJsonDocument<256> body;
  DeserializationError de = deserializeJson(body, server.arg("plain"));
  if (de) {
    server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }
  String mode = body["mode"] | String("");
  if (!mode.length()) {
    server.send(400, "application/json", "{\"error\":\"missing_mode\"}");
    return;
  }
  if (!wss_nfc_provision_set_mode(mode.c_str())) {
    server.send(409, "application/json", "{\"error\":\"provision_mode_failed\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_nfc_provision_stop() {
  if (!admin_required("nfc_provision_stop")) return;
  wss_nfc_provision_stop("admin_stop");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void serve_file_or_404(const String& path, const String& contentType) {
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  File f = LittleFS.open(path, "r");
  server.streamFile(f, contentType);
  f.close();
}

static String content_type_for(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

void wss_web_begin(WssConfigStore& cfg, WssEventLogger& log) {
  g_cfg = &cfg;
  g_log = &log;

  server.on("/api/status", HTTP_GET, handle_status);
  server.on("/api/events", HTTP_GET, handle_events);
  server.on("/api/logs/list", HTTP_GET, handle_logs_list);
  server.on("/api/logs/download", HTTP_GET, handle_logs_download);

  // Admin session
  server.on("/api/admin/status", HTTP_GET, handle_admin_status);
  server.on("/api/admin/login", HTTP_POST, handle_admin_login);
  server.on("/api/admin/logout", HTTP_POST, handle_admin_logout);
  server.on("/api/admin/eligible/clear", HTTP_POST, handle_admin_eligible_clear);

  // M2: time setting (RTC adjust) – gated after setup.
  server.on("/api/time/set", HTTP_POST, handle_time_set);

  // Wizard
  server.on("/api/wizard/status", HTTP_GET, handle_wizard_status);
  server.on("/api/wizard/step", HTTP_POST, handle_wizard_set_step);
  server.on("/api/wizard/complete", HTTP_POST, handle_wizard_complete);

  // Config (admin only)
  server.on("/api/config", HTTP_GET, handle_config_get);
  server.on("/api/config", HTTP_POST, handle_config_post);

  // Factory restore
  server.on("/api/factory_restore", HTTP_POST, handle_factory_restore);

  // M4: Control parity (admin only in V1).
  server.on("/api/control/arm", HTTP_POST, []() { if (!admin_required("control_arm")) return; handle_control_action("arm"); });
  server.on("/api/control/disarm", HTTP_POST, []() { if (!admin_required("control_disarm")) return; handle_control_action("disarm"); });
  server.on("/api/control/silence", HTTP_POST, []() { if (!admin_required("control_silence")) return; handle_control_action("silence"); });

  // M6: NFC provisioning (admin only).
  server.on("/api/nfc/provision/start", HTTP_POST, handle_nfc_provision_start);
  server.on("/api/nfc/provision/mode", HTTP_POST, handle_nfc_provision_mode);
  server.on("/api/nfc/provision/stop", HTTP_POST, handle_nfc_provision_stop);

  // UI root
  server.on("/", HTTP_GET, []() {
    if (!wss_flash_fs_has_index()) {
      server.send(503, "text/plain",
        "UI assets missing in flash filesystem. Upload FS image (LittleFS) and retry.");
      return;
    }
    serve_file_or_404("/index.html", "text/html");
  });

  // Static file handler (simple SPA)
  server.onNotFound([]() {
    String path = server.uri();
    if (path == "/") {
      server.send(302, "text/plain", "Redirect");
      return;
    }
    // Try exact file
    if (LittleFS.exists(path)) {
      serve_file_or_404(path, content_type_for(path));
      return;
    }
    // SPA fallback to index.html for unknown routes
    if (wss_flash_fs_has_index()) {
      serve_file_or_404("/index.html", "text/html");
      return;
    }
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
}

void wss_web_loop() {
  server.handleClient();
}
