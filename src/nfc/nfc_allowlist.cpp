// src/nfc/nfc_allowlist.cpp
// Role: NFC allowlist storage interface (M6 slice 1).

#include "nfc_allowlist.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>

#include "../logging/event_logger.h"
#include "../logging/sha256_hex.h"
#include "../storage/storage_manager.h"

namespace {

struct AllowEntry {
  String taghash;
  WssNfcRole role;
};

static std::vector<AllowEntry> g_allowlist;
static const char* kPrefsNs = "wss_nfc_allow";
static const char* kPrefsKey = "entries_json";
static const uint32_t kAllowlistSchemaVersion = 1;

static uint64_t device_salt() {
  return ESP.getEfuseMac();
}

static bool parse_allowlist_json(const String& payload, WssEventLogger* log) {
  DynamicJsonDocument d(4096);
  DeserializationError de = deserializeJson(d, payload);
  if (de) return false;

  JsonArray arr;
  if (d.is<JsonArray>()) {
    arr = d.as<JsonArray>();
  } else {
    arr = d["entries"].as<JsonArray>();
  }
  if (arr.isNull()) return false;

  g_allowlist.clear();
  for (JsonVariantConst v : arr) {
    JsonObjectConst o = v.as<JsonObjectConst>();
    if (o.isNull()) continue;
    String taghash = o["tag"] | String("");
    String role_str = o["role"] | String("");
    if (!taghash.length()) continue;
    WssNfcRole role = WSS_NFC_ROLE_UNKNOWN;
    role_str.toLowerCase();
    if (role_str == "admin") role = WSS_NFC_ROLE_ADMIN;
    else if (role_str == "user") role = WSS_NFC_ROLE_USER;
    g_allowlist.push_back({taghash, role});
  }
  if (log) log->log_info("nfc", "allowlist_loaded", "allowlist loaded");
  return true;
}

static bool load_allowlist_from_nvs(WssEventLogger* log) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNs, true)) return false;
  String payload = prefs.getString(kPrefsKey, "");
  prefs.end();
  if (!payload.length()) return false;
  return parse_allowlist_json(payload, log);
}

static String build_allowlist_json() {
  DynamicJsonDocument d(4096);
  d["version"] = kAllowlistSchemaVersion;
  JsonArray arr = d.createNestedArray("entries");
  for (const auto& e : g_allowlist) {
    JsonObject o = arr.createNestedObject();
    o["tag"] = e.taghash;
    o["role"] = wss_nfc_role_to_string(e.role);
  }
  String out;
  serializeJson(d, out);
  return out;
}

static bool save_allowlist_to_nvs(const String& payload) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNs, false)) return false;
  bool ok = prefs.putString(kPrefsKey, payload) > 0;
  prefs.end();
  return ok;
}

} // namespace

bool wss_nfc_allowlist_begin(WssEventLogger* log) {
  String err;
  String payload;
  if (wss_storage_read_allowlist(payload, err)) {
    if (parse_allowlist_json(payload, log)) return true;
    if (log) log->log_warn("nfc", "allowlist_parse_failed", "allowlist parse failed; falling back to NVS");
  } else {
    if (log && err.length()) {
      StaticJsonDocument<128> extra;
      extra["error"] = err;
      JsonObjectConst o = extra.as<JsonObjectConst>();
      log->log_warn("nfc", "allowlist_sd_unavailable", "allowlist SD unavailable; using NVS", &o);
    }
  }
  return load_allowlist_from_nvs(log);
}

String wss_nfc_taghash(const uint8_t* uid, size_t uid_len) {
  if (!uid || uid_len == 0) return String();
  uint8_t* data = nullptr;
  size_t len = 0;
  uint8_t stack_buf[8 + 16];
  if (uid_len <= 16) {
    data = stack_buf;
    len = 8 + uid_len;
  } else {
    data = new uint8_t[8 + uid_len];
    len = 8 + uid_len;
  }

  uint64_t salt = device_salt();
  for (int i = 0; i < 8; i++) {
    data[i] = (uint8_t)((salt >> (56 - i * 8)) & 0xFF);
  }
  memcpy(data + 8, uid, uid_len);
  String out = wss_sha256_hex(data, len);
  if (data != stack_buf) {
    delete[] data;
  }
  return out;
}

bool wss_nfc_allowlist_is_allowed(const String& taghash) {
  return wss_nfc_allowlist_get_role(taghash) != WSS_NFC_ROLE_UNKNOWN;
}

WssNfcRole wss_nfc_allowlist_get_role(const String& taghash) {
  if (!taghash.length()) return WSS_NFC_ROLE_UNKNOWN;
  for (const auto& e : g_allowlist) {
    if (taghash == e.taghash) return e.role;
  }
  return WSS_NFC_ROLE_UNKNOWN;
}

const char* wss_nfc_role_to_string(WssNfcRole role) {
  switch (role) {
    case WSS_NFC_ROLE_ADMIN:
      return "admin";
    case WSS_NFC_ROLE_USER:
      return "user";
    case WSS_NFC_ROLE_UNKNOWN:
    default:
      return "unknown";
  }
}

bool wss_nfc_allowlist_add(const String& taghash, WssNfcRole role, WssEventLogger* log) {
  if (!taghash.length()) return false;
  bool changed = false;
  for (auto& e : g_allowlist) {
    if (e.taghash == taghash) {
      if (e.role != role) {
        e.role = role;
        changed = true;
      }
      String payload = build_allowlist_json();
      (void)save_allowlist_to_nvs(payload);
      String err;
      if (!wss_storage_write_allowlist(payload, err) && log && err.length()) {
        StaticJsonDocument<128> extra;
        extra["error"] = err;
        JsonObjectConst o = extra.as<JsonObjectConst>();
        log->log_warn("nfc", "allowlist_sd_write_failed", "allowlist SD write failed", &o);
      }
      return changed;
    }
  }
  g_allowlist.push_back({taghash, role});
  changed = true;
  String payload = build_allowlist_json();
  (void)save_allowlist_to_nvs(payload);
  String err;
  if (!wss_storage_write_allowlist(payload, err) && log && err.length()) {
    StaticJsonDocument<128> extra;
    extra["error"] = err;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    log->log_warn("nfc", "allowlist_sd_write_failed", "allowlist SD write failed", &o);
  }
  return changed;
}

bool wss_nfc_allowlist_remove(const String& taghash, WssEventLogger* log) {
  if (!taghash.length()) return false;
  bool removed = false;
  for (size_t i = 0; i < g_allowlist.size(); i++) {
    if (g_allowlist[i].taghash == taghash) {
      g_allowlist.erase(g_allowlist.begin() + i);
      removed = true;
      break;
    }
  }
  String payload = build_allowlist_json();
  (void)save_allowlist_to_nvs(payload);
  String err;
  if (!wss_storage_write_allowlist(payload, err) && log && err.length()) {
    StaticJsonDocument<128> extra;
    extra["error"] = err;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    log->log_warn("nfc", "allowlist_sd_write_failed", "allowlist SD write failed", &o);
  }
  return removed;
}

void wss_nfc_allowlist_factory_reset(WssEventLogger& log) {
  g_allowlist.clear();
  String payload = build_allowlist_json();
  (void)save_allowlist_to_nvs(payload);
  String err;
  if (!wss_storage_write_allowlist(payload, err) && err.length()) {
    StaticJsonDocument<128> extra;
    extra["error"] = err;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    log.log_warn("nfc", "allowlist_sd_write_failed", "allowlist SD write failed", &o);
  }
  log.log_info("nfc", "allowlist_factory_reset", "allowlist reset");
}
