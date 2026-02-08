// src/config/config_store.cpp
// Role: Implementation of schema-versioned ConfigStore persisted in NVS.

#include "config_store.h"

#include <Preferences.h>
#include <mbedtls/sha256.h>

#include "../logging/event_logger.h"
#include "version.h"

static const char* kPrefsNamespace = "wss";
static const char* kPrefsKeyCfg = "cfg_json";
static const char* kPrefsKeyCfgChunks = "cfg_chunks";
static const char* kPrefsKeyCfgChunkPrefix = "cfg_chunk_";
static const size_t kCfgSingleMaxBytes = 1800;
static const size_t kCfgChunkBytes = 1024;
static const uint32_t kCfgChunkMax = 16;

static String cfg_chunk_key(uint32_t idx) {
  return String(kPrefsKeyCfgChunkPrefix) + String(idx);
}

static void clear_cfg_chunks(Preferences& prefs, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    String key = cfg_chunk_key(i);
    prefs.remove(key.c_str());
  }
  prefs.remove(kPrefsKeyCfgChunks);
}

static bool json_equals(const JsonVariantConst& a, const JsonVariantConst& b) {
  if (a.isNull() && b.isNull()) return true;
  if (a.is<const char*>() && b.is<const char*>()) {
    return String(a.as<const char*>()) == String(b.as<const char*>());
  }
  if (a.is<String>() && b.is<String>()) {
    return a.as<String>() == b.as<String>();
  }
  if (a.is<bool>() && b.is<bool>()) return a.as<bool>() == b.as<bool>();
  if (a.is<long>() && b.is<long>()) return a.as<long>() == b.as<long>();
  if (a.is<double>() && b.is<double>()) return a.as<double>() == b.as<double>();
  // Fallback: compare serialized forms
  String sa, sb;
  serializeJson(a, sa);
  serializeJson(b, sb);
  return sa == sb;
}

bool WssConfigStore::begin(const String& device_suffix, WssEventLogger* logger) {
  _device_suffix = device_suffix;
  _logger = logger;
  String err;
  if (!load(err)) {
    // load() performs recovery attempts and sets defaults if needed
    _ok = false;
    if (_logger) _logger->log_warn("config", "config_load_failed", err);
    return false;
  }
  ensure_runtime_defaults();
  _ok = true;
  return true;
}

bool WssConfigStore::load(String& err) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    err = "prefs_begin_failed";
    set_defaults();
    save(err); // best-effort
    if (_logger) {
      StaticJsonDocument<128> extra;
      extra["reason"] = err;
      JsonObjectConst o = extra.as<JsonObjectConst>();
      _logger->log_warn("config", "cfg_load_missing", "config load missing", &o);
    }
    return false;
  }

  String cfg;
  bool used_chunked = false;
  uint32_t chunk_count = prefs.getUInt(kPrefsKeyCfgChunks, 0);
  if (chunk_count > 0 && chunk_count <= kCfgChunkMax) {
    for (uint32_t i = 0; i < chunk_count; i++) {
      String key = cfg_chunk_key(i);
      String part = prefs.getString(key.c_str(), "");
      if (part.length() == 0) {
        cfg = "";
        break;
      }
      cfg += part;
    }
    if (cfg.length() > 0) used_chunked = true;
  }

  if (!used_chunked) {
    cfg = prefs.getString(kPrefsKeyCfg, "");
  }
  prefs.end();

  if (cfg.length() == 0) {
    set_defaults();
    String save_err;
    save(save_err); // best-effort
    if (_logger) _logger->log_info("config", "config_defaults_created", "no_existing_config");
    if (_logger) _logger->log_warn("config", "cfg_load_missing", "config load missing");
    err = "";
    return true;
  }

  _doc.clear();
  DeserializationError de = deserializeJson(_doc, cfg);
  if (de) {
    err = String("deserialize_failed:") + de.c_str();
    // Corrupt recovery: reset to defaults and require wizard.
    set_defaults();
    String save_err;
    save(save_err);
    if (_logger) _logger->log_error("config", "config_corrupt_recovered", err);
    if (_logger) {
      StaticJsonDocument<128> extra;
      extra["reason"] = "deserialize_failed";
      JsonObjectConst o = extra.as<JsonObjectConst>();
      _logger->log_warn("config", "cfg_load_missing", "config load missing", &o);
    }
    err = "";
    return true;
  }

  // Validate/migrate
  if (!validate_or_recover(err)) {
    // validate_or_recover sets defaults on failure
    String save_err;
    save(save_err);
    if (_logger) _logger->log_error("config", "config_invalid_recovered", err);
    if (_logger) {
      StaticJsonDocument<128> extra;
      extra["reason"] = "validate_failed";
      JsonObjectConst o = extra.as<JsonObjectConst>();
      _logger->log_warn("config", "cfg_load_missing", "config load missing", &o);
    }
    err = "";
    return true;
  }

  if (_logger) _logger->log_info("config", "cfg_load_ok", "config load ok");
  err = "";
  return true;
}

void WssConfigStore::set_defaults() {
  _doc.clear();
  JsonObject root = _doc.to<JsonObject>();

  root["schema_version"] = (uint32_t)WSS_CONFIG_SCHEMA_VERSION;

  // Setup Wizard gating
  root["setup_completed"] = false;
  root["setup_wizard_version"] = 1;
  root["setup_last_step"] = "welcome";

  // System
  root["factory_restore_enabled"] = true;
  root["control_web_enabled"] = true;
  root["control_nfc_enabled"] = true;
  root["device_name"] = "Workshop Security System";
  root["timezone"] = "";
  root["admin_mode_timeout_s"] = 600;

  // Wi-Fi
  root["wifi_sta_enabled"] = false;
  root["wifi_sta_ssid"] = "";
  root["wifi_sta_password"] = "";
  root["wifi_sta_connect_timeout_s"] = 20;

  root["wifi_ap_ssid_base"] = "Workshop Security System";
  root["wifi_ap_suffix_enabled"] = true;
  root["wifi_ap_ssid"] = ""; // derived
  root["wifi_ap_password"] = ""; // derived during runtime defaults

  // NFC optional URL record
  root["nfc_url_record_enabled"] = false;
  root["nfc_url_record_preserve_if_possible"] = true;
  root["nfc_url"] = "http://192.168.4.1/";

  // NFC interface + pins (M7.3 SPI defaults)
  root["nfc_interface"] = "spi";
  root["nfc_spi_cs_gpio"] = 27;
  root["nfc_spi_rst_gpio"] = 33;
  root["nfc_spi_irq_gpio"] = 32;

  // NFC / access (scaffolding)
  root["allow_user_arm"] = true;
  root["allow_user_disarm"] = true;
  root["allow_user_silence"] = true;
  root["invalid_scan_window_s"] = 30;
  root["invalid_scan_max"] = 5;
  root["lockout_duration_s"] = 60;

  // Outputs (scaffolding)
  root["silenced_duration_s"] = 180;
  root["horn_enabled"] = true;
  root["light_enabled"] = true;
  root["horn_pattern"] = "steady";
  root["light_pattern"] = "steady";
  root["silenced_light_pattern"] = "steady";

  // Sensors (scaffolding)
  root["required_primary_sensor"] = "motion";
  root["motion_sensors_max"] = 2;
  root["door_sensors_max"] = 2;
  root["enclosure_open_enabled"] = false;
  root["motion_enabled"] = true;
  // M5: per-sensor enables (legacy motion_enabled/door_enabled remain for backwards compatibility)
  root["motion1_enabled"] = true;
  root["motion2_enabled"] = false;
  root["motion_sensitivity"] = 0;
  // M7.3: motion sensor interface selection (GPIO vs LD2410B UART).
  root["motion_kind"] = "gpio";
  root["motion_ld2410b_rx_gpio"] = 16;
  root["motion_ld2410b_tx_gpio"] = 17;
  root["motion_ld2410b_baud"] = 256000;
  root["door_enabled"] = false;
  root["door1_enabled"] = false;
  root["door2_enabled"] = false;
  root["tamper_enabled"] = false;
  root["armed_present_mode_enabled"] = false;

  // M5: digital sensor interpretation knobs (used only when a corresponding pin is configured)
  // Allowed values:
  //   *_pull: pullup|pulldown|floating
  //   *_active_level: high|low
  root["motion1_pull"] = "floating";
  root["motion1_active_level"] = "high";
  root["motion2_pull"] = "floating";
  root["motion2_active_level"] = "high";

  root["door1_pull"] = "pullup";
  root["door1_active_level"] = "high";
  root["door2_pull"] = "pullup";
  root["door2_active_level"] = "high";

  root["enclosure1_pull"] = "pullup";
  root["enclosure1_active_level"] = "high";

  // Storage
  root["sd_enabled"] = true;
  root["sd_cs_gpio"] = 13;
  root["sd_required"] = false;
  root["log_retention_days"] = 365;
  root["hash_chain_logs"] = true;
  root["factory_restore_wipes_logs"] = false;
  root["factory_restore_wipes_allowlist"] = true;
  root["factory_restore_requires_hold"] = true;

  // Power (scaffolding)
  root["battery_measure_enabled"] = false;
  root["battery_low_v"] = 0.0;
  root["battery_critical_v"] = 0.0;
  root["battery_wifi_disable_v"] = 0.0;

  // Admin web password (stored as SHA-256 hex). Empty means "not set".
  root["admin_web_password_hash"] = "";
}

bool WssConfigStore::validate_or_recover(String& err) {
  JsonObject root = _doc.as<JsonObject>();
  if (root.isNull()) {
    err = "root_not_object";
    set_defaults();
    return false;
  }

  uint32_t schema = root["schema_version"] | 0;
  if (schema == 0) {
    // treat missing as v1
    schema = WSS_CONFIG_SCHEMA_VERSION;
    root["schema_version"] = schema;
  }

  if (schema != (uint32_t)WSS_CONFIG_SCHEMA_VERSION) {
    // M1 supports only v1 baseline; migration hook exists but no other versions expected yet.
    if (!migrate_if_needed(schema, (uint32_t)WSS_CONFIG_SCHEMA_VERSION, err)) {
      // Recovery: defaults + wizard required.
      String detail = err;
      set_defaults();
      err = String("schema_incompatible:") + detail;
      return false;
    }
  }

  // Ensure wizard flags exist.
  if (!root.containsKey("setup_completed")) root["setup_completed"] = false;
  if (!root.containsKey("setup_last_step")) root["setup_last_step"] = "welcome";

  // Ensure expected types for a minimal set of keys.
  // (Append-only rule: unknown keys are tolerated.)
  if (!root["admin_mode_timeout_s"].is<long>()) root["admin_mode_timeout_s"] = 600;
  if (!root["control_web_enabled"].is<bool>()) root["control_web_enabled"] = true;
  if (!root["control_nfc_enabled"].is<bool>()) root["control_nfc_enabled"] = true;
  if (!root.containsKey("nfc_interface") || !root["nfc_interface"].is<const char*>()) {
    root["nfc_interface"] = "spi";
  }
  if (!root["nfc_spi_cs_gpio"].is<long>()) root["nfc_spi_cs_gpio"] = 27;
  if (!root["nfc_spi_rst_gpio"].is<long>()) root["nfc_spi_rst_gpio"] = 33;
  if (!root["nfc_spi_irq_gpio"].is<long>()) root["nfc_spi_irq_gpio"] = 32;

  // M5: ensure per-sensor keys exist for older configs.
  if (!root["motion_enabled"].is<bool>()) root["motion_enabled"] = true;
  if (!root["door_enabled"].is<bool>()) root["door_enabled"] = false;

  if (!root["motion1_enabled"].is<bool>()) root["motion1_enabled"] = (bool)(root["motion_enabled"] | true);
  if (!root["motion2_enabled"].is<bool>()) root["motion2_enabled"] = false;
  if (!root["door1_enabled"].is<bool>()) root["door1_enabled"] = (bool)(root["door_enabled"] | false);
  if (!root["door2_enabled"].is<bool>()) root["door2_enabled"] = false;

  if (!root.containsKey("motion_kind") || !root["motion_kind"].is<const char*>()) {
    root["motion_kind"] = "gpio";
  }
  if (!root["motion_ld2410b_rx_gpio"].is<long>()) root["motion_ld2410b_rx_gpio"] = 16;
  if (!root["motion_ld2410b_tx_gpio"].is<long>()) root["motion_ld2410b_tx_gpio"] = 17;
  if (!root["motion_ld2410b_baud"].is<long>()) root["motion_ld2410b_baud"] = 256000;

  if (!root.containsKey("motion1_pull") || !root["motion1_pull"].is<const char*>()) root["motion1_pull"] = "floating";
  if (!root.containsKey("motion1_active_level") || !root["motion1_active_level"].is<const char*>()) root["motion1_active_level"] = "high";
  if (!root.containsKey("motion2_pull") || !root["motion2_pull"].is<const char*>()) root["motion2_pull"] = "floating";
  if (!root.containsKey("motion2_active_level") || !root["motion2_active_level"].is<const char*>()) root["motion2_active_level"] = "high";

  if (!root.containsKey("door1_pull") || !root["door1_pull"].is<const char*>()) root["door1_pull"] = "pullup";
  if (!root.containsKey("door1_active_level") || !root["door1_active_level"].is<const char*>()) root["door1_active_level"] = "high";
  if (!root.containsKey("door2_pull") || !root["door2_pull"].is<const char*>()) root["door2_pull"] = "pullup";
  if (!root.containsKey("door2_active_level") || !root["door2_active_level"].is<const char*>()) root["door2_active_level"] = "high";

  if (!root.containsKey("enclosure1_pull") || !root["enclosure1_pull"].is<const char*>()) root["enclosure1_pull"] = "pullup";
  if (!root.containsKey("enclosure1_active_level") || !root["enclosure1_active_level"].is<const char*>()) root["enclosure1_active_level"] = "high";

  if (!root["sd_enabled"].is<bool>()) root["sd_enabled"] = true;
  if (!root["sd_cs_gpio"].is<long>()) root["sd_cs_gpio"] = 13;
  if (!root["sd_required"].is<bool>()) root["sd_required"] = false;

  return true;
}

bool WssConfigStore::migrate_if_needed(uint32_t from_version, uint32_t to_version, String& err) {
  // v1.x migration framework placeholder.
  // M1 currently expects schema_version==1. A no-op migration is allowed for 1->1.
  if (from_version == to_version) return true;
  if (from_version == 1 && to_version == 1) return true;
  err = String("no_migration_path_") + from_version + "_to_" + to_version;
  return false;
}

void WssConfigStore::ensure_runtime_defaults() {
  JsonObject root = _doc.as<JsonObject>();

  // Derived AP SSID and provisioning password defaults.
  String base = root["wifi_ap_ssid_base"] | String("Workshop Security System");
  bool suffix_en = root["wifi_ap_suffix_enabled"] | true;
  String ssid = suffix_en ? (base + " - " + _device_suffix) : base;
  root["wifi_ap_ssid"] = ssid;

  // If AP password hasn't been set yet, use the predictable provisioning default.
  String ap_pass = root["wifi_ap_password"] | String("");
  if (ap_pass.length() < 8) {
    root["wifi_ap_password"] = String("ChangeMe-") + _device_suffix;
  }
}

bool WssConfigStore::setup_completed() const {
  return _doc["setup_completed"].is<bool>() ? _doc["setup_completed"].as<bool>() : false;
}

String WssConfigStore::setup_last_step() const {
  if (_doc["setup_last_step"].is<const char*>()) return String(_doc["setup_last_step"].as<const char*>());
  return String("welcome");
}

bool WssConfigStore::admin_password_set() const {
  String h = _doc["admin_web_password_hash"] | String("");
  return h.length() == 64;
}

bool WssConfigStore::ap_password_is_default() const {
  String ap_pass = _doc["wifi_ap_password"] | String("");
  String derived = String("ChangeMe-") + _device_suffix;
  return ap_pass == derived;
}

bool WssConfigStore::verify_admin_password(const String& candidate) const {
  String stored = _doc["admin_web_password_hash"] | String("");
  if (stored.length() != 64) return false;
  return sha256_hex(candidate) == stored;
}

bool WssConfigStore::is_secret_key(const String& key) const {
  return key == "wifi_sta_password" || key == "wifi_ap_password" || key == "admin_web_password_hash";
}

String WssConfigStore::sha256_hex(const String& s) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish_ret(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  static const char* hex = "0123456789abcdef";
  char out[65];
  for (int i = 0; i < 32; i++) {
    out[i*2] = hex[(hash[i] >> 4) & 0xF];
    out[i*2+1] = hex[hash[i] & 0xF];
  }
  out[64] = 0;
  return String(out);
}

void WssConfigStore::to_redacted_json(JsonDocument& out) const {
  out.clear();
  JsonObject o = out.to<JsonObject>();
  for (JsonPairConst kv : _doc.as<JsonObjectConst>()) {
    String key = kv.key().c_str();
    if (is_secret_key(key)) {
      o[key] = "***";
    } else {
      o[key] = kv.value();
    }
  }
}

bool WssConfigStore::apply_patch(const JsonObjectConst& patch, String& err, JsonArray changed_keys_out) {
  if (patch.isNull()) {
    err = "patch_not_object";
    return false;
  }

  JsonObject root = _doc.as<JsonObject>();
  bool changed = false;

  for (JsonPairConst kv : patch) {
    String key = kv.key().c_str();
    if (!root.containsKey(key)) {
      // Unknown keys are rejected to reduce surprise in M1.
      continue;
    }

    if (!json_equals(root[key], kv.value())) {
      // Special handling: admin password is set via cleartext in API, but stored hashed.
      if (key == "admin_web_password") {
        // Not part of canonical keys; ignore.
        continue;
      }
      root[key] = kv.value();
      changed = true;
      changed_keys_out.add(key);
    }
  }

  if (changed) {
    ensure_runtime_defaults();
  }

  err = "";
  return changed;
}



bool WssConfigStore::wizard_set(const char* key, const char* value, String& err) {
  JsonObject root = _doc.as<JsonObject>();
  if (!root.containsKey(key)) { err = "unknown_key"; return false; }

  if (String(key) == "admin_web_password") {
    String pw = value ? String(value) : String("");
    if (pw.length() < 8) { err = "admin_password_min_8"; return false; }
    root["admin_web_password_hash"] = sha256_hex(pw);
    err = "";
    return true;
  }

  root[key] = value;
  ensure_runtime_defaults();
  err = "";
  return true;
}



bool WssConfigStore::wizard_set(const char* key, const String& value, String& err) {
  return wizard_set(key, value.c_str(), err);
}



bool WssConfigStore::wizard_set(const char* key, bool value, String& err) {
  JsonObject root = _doc.as<JsonObject>();
  if (!root.containsKey(key)) { err = "unknown_key"; return false; }

  root[key] = value;
  ensure_runtime_defaults();
  err = "";
  return true;
}


bool WssConfigStore::wizard_set_variant(const char* key, const JsonVariantConst& value, String& err) {
  JsonObject root = _doc.as<JsonObject>();
  if (String(key) == "admin_web_password") {
    String pw = value.is<const char*>() ? String(value.as<const char*>()) : String("");
    if (pw.length() < 8) {
      err = "admin_password_min_8";
      return false;
    }
    root["admin_web_password_hash"] = sha256_hex(pw);
    err = "";
    return true;
  }

  if (!root.containsKey(key)) {
    err = "unknown_key";
    return false;
  }

  root[key] = value;
  ensure_runtime_defaults();
  err = "";
  return true;
}

bool WssConfigStore::save(String& err) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    err = "prefs_begin_failed";
    return false;
  }

  String out;
  size_t written = serializeJson(_doc, out);
  if (written == 0 || out.length() == 0) {
    prefs.end();
    err = "serialize_failed";
    return false;
  }

  bool ok = false;
  if (out.length() <= kCfgSingleMaxBytes) {
    ok = prefs.putString(kPrefsKeyCfg, out) > 0;
    if (ok) {
      uint32_t prior_chunks = prefs.getUInt(kPrefsKeyCfgChunks, 0);
      if (prior_chunks > 0) clear_cfg_chunks(prefs, prior_chunks);
    }
  }

  if (!ok) {
    uint32_t chunk_count = (out.length() + kCfgChunkBytes - 1) / kCfgChunkBytes;
    if (chunk_count == 0 || chunk_count > kCfgChunkMax) {
      prefs.end();
      err = "cfg_too_large";
      return false;
    }

    uint32_t prior_chunks = prefs.getUInt(kPrefsKeyCfgChunks, 0);
    prefs.putUInt(kPrefsKeyCfgChunks, 0);
    bool chunk_ok = true;
    for (uint32_t i = 0; i < chunk_count; i++) {
      size_t start = i * kCfgChunkBytes;
      size_t end = start + kCfgChunkBytes;
      if (end > out.length()) end = out.length();
      String part = out.substring(start, end);
      String key = cfg_chunk_key(i);
      if (prefs.putString(key.c_str(), part) == 0) {
        chunk_ok = false;
        break;
      }
    }
    if (chunk_ok) {
      if (prefs.putUInt(kPrefsKeyCfgChunks, chunk_count) == 0) {
        chunk_ok = false;
      }
    }
    if (chunk_ok) {
      prefs.remove(kPrefsKeyCfg);
      if (prior_chunks > chunk_count) {
        for (uint32_t i = chunk_count; i < prior_chunks; i++) {
          String key = cfg_chunk_key(i);
          prefs.remove(key.c_str());
        }
      }
      ok = true;
    }
  }
  prefs.end();

  if (!ok) {
    err = "prefs_put_failed";
    return false;
  }
  err = "";
  return true;
}

bool WssConfigStore::factory_reset(String& err) {
  set_defaults();
  ensure_runtime_defaults();
  return save(err);
}
